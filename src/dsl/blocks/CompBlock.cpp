#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <cmath>

using namespace dsl;

void SignalChain::Comp::clearRuntimeState() noexcept
{
    envDb = 0.f;
    rmsState = 0.f;
    writePos = 0;
    hpfLpL = 0.f;
    hpfLpR = 0.f;
    hpfLpL2 = 0.f;
    hpfLpR2 = 0.f;
    cachedAtk = cachedRel = cachedHpf = -1.f;
    cachedMakeup = cachedCeil = 1.0e9f;
    ceilLin = 1.f;
}

void SignalChain::Comp::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    auto resetSm = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fallback)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fallback);
    };
    resetSm (thrSm, threshold, -12.f);
    resetSm (ratioSm, ratio, 4.f);
    resetSm (atkSm, attack, 0.01f);
    resetSm (relSm, release, 0.1f);
    resetSm (kneeSm, kneeDb, 0.f);
    resetSm (makeupSm, makeupDb, 0.f);
    resetSm (hpfSm, hpfHz, 0.f);
    resetSm (ceilSm, ceilingDb, 0.f);
    resetSm (mixSm, mixExpr, 1.f);
    rmsC = 1.f - std::exp (-1.f / (0.01f * sampleRate));
    float aheadMs = lookaheadMs.evaluate (0.f);
    if (! std::isfinite (aheadMs))
        aheadMs = 0.f;
    aheadMs = juce::jlimit (0.f, kMaxLookaheadSec * 1000.f, aheadMs);
    const int ahead = (int) std::lround ((double) aheadMs * 0.001 * (double) sampleRate);
    delayN = juce::jmax (8, (int) std::ceil ((double) sampleRate * (double) kMaxLookaheadSec) + 4);
    delayL = DSPUtils::alignedRing (storageL, delayN);
    delayR = DSPUtils::alignedRing (storageR, delayN);
    latencySamples = (ahead >= 2 && ahead <= delayN - 2) ? ahead : 0;
    clearRuntimeState();
    ceilLin = juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 0.f, ceilSm.getCurrentValue()));
    bindings.prepare (varPtr, { &threshold, &ratio, &attack, &release, &kneeDb, &makeupDb, &hpfHz, &ceilingDb, &mixExpr, &lookaheadMs });
    yPtr = nullptr;
    if (varPtr != nullptr)
    {
        yPtr = &(*varPtr)["y"];
    }
}

float SignalChain::Comp::computeGrDb (float levelDb, float thrDb, float ratio, float knee) const noexcept
{
    const float slope = 1.f - 1.f / juce::jmax (1.001f, ratio);
    const float over = levelDb - thrDb;
    if (knee <= 0.05f)
        return over > 0.f ? over * slope : 0.f;

    const float half = 0.5f * knee;
    if (over <= -half)
        return 0.f;
    if (over >= half)
        return over * slope;
    const float x = over + half;
    return slope * (x * x) / (2.f * knee);
}

float SignalChain::Comp::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::AudioBuffer<float> one (1, 1);
    one.setSample (0, 0, x);
    processBlock (one);
    return one.getSample (0, 0);
}

void SignalChain::Comp::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    auto ev = [] (ExpressionEvaluator& e, float fallback)
    {
        const float v = e.evaluateLive (0.f);
        return std::isfinite (v) ? v : fallback;
    };
    thrSm.setTargetValue (ev (threshold, -12.f));
    ratioSm.setTargetValue (ev (ratio, 4.f));
    atkSm.setTargetValue (ev (attack, 0.01f));
    relSm.setTargetValue (ev (release, 0.1f));
    kneeSm.setTargetValue (ev (kneeDb, 0.f));
    makeupSm.setTargetValue (ev (makeupDb, 0.f));
    hpfSm.setTargetValue (ev (hpfHz, 0.f));
    ceilSm.setTargetValue (ev (ceilingDb, 0.f));
    mixSm.setTargetValue (ev (mixExpr, 1.f));

    float* out[2] {};
    const int useCh = juce::jmin (nCh, 2);
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    const bool live = thrSm.isSmoothing() || ratioSm.isSmoothing() || atkSm.isSmoothing()
                   || relSm.isSmoothing() || kneeSm.isSmoothing() || makeupSm.isSmoothing()
                   || hpfSm.isSmoothing() || ceilSm.isSmoothing() || mixSm.isSmoothing();

    auto refreshCached = [this] (float atk, float rel, float hpf, float makeup, float ceilDb) noexcept
    {
        if (std::abs (atk - cachedAtk) > 1.0e-6f)
        {
            cachedAtk = atk;
            atkC = 1.f - std::exp (-1.f / (atk * sampleRate));
        }
        if (std::abs (rel - cachedRel) > 1.0e-6f)
        {
            cachedRel = rel;
            relC = 1.f - std::exp (-1.f / (rel * sampleRate));
        }
        if (std::abs (hpf - cachedHpf) > 1.0e-3f)
        {
            cachedHpf = hpf;
            hpfA = (hpf > 8.f)
                ? std::exp (-2.f * juce::MathConstants<float>::pi * hpf / sampleRate)
                : 0.f;
        }
        if (std::abs (makeup - cachedMakeup) > 1.0e-4f)
        {
            cachedMakeup = makeup;
            makeupLin = juce::Decibels::decibelsToGain (makeup);
        }
        if (std::abs (ceilDb - cachedCeil) > 1.0e-4f)
        {
            cachedCeil = ceilDb;
            ceilLin = juce::Decibels::decibelsToGain (ceilDb);
        }
    };

    float thr = juce::jlimit (-80.f, 0.f, thrSm.getCurrentValue());
    float rat = juce::jlimit (1.f, 40.f, ratioSm.getCurrentValue());
    float knee = juce::jlimit (0.f, 24.f, kneeSm.getCurrentValue());
    float hpf = juce::jlimit (0.f, 800.f, hpfSm.getCurrentValue());
    float mix = juce::jlimit (0.f, 1.f, mixSm.getCurrentValue());
    refreshCached (juce::jmax (0.00005f, atkSm.getCurrentValue()),
                   juce::jmax (0.005f, relSm.getCurrentValue()),
                   hpf,
                   juce::jlimit (-24.f, 24.f, makeupSm.getCurrentValue()),
                   juce::jlimit (-24.f, 0.f, ceilSm.getCurrentValue()));

    for (int i = 0; i < nS; ++i)
    {
        if (live)
        {
            thr = juce::jlimit (-80.f, 0.f, thrSm.getNextValue());
            rat = juce::jlimit (1.f, 40.f, ratioSm.getNextValue());
            mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
            const float atk = juce::jmax (0.00005f, atkSm.getNextValue());
            const float rel = juce::jmax (0.005f, relSm.getNextValue());
            knee = juce::jlimit (0.f, 24.f, kneeSm.getNextValue());
            const float makeup = juce::jlimit (-24.f, 24.f, makeupSm.getNextValue());
            hpf = juce::jlimit (0.f, 800.f, hpfSm.getNextValue());
            const float ceilDb = juce::jlimit (-24.f, 0.f, ceilSm.getNextValue());
            refreshCached (atk, rel, hpf, makeup, ceilDb);
        }

        float detL = 0.f, detR = 0.f;
        if (followSidechain)
        {
            if (scL != nullptr && i < scN)
            {
                const int si = i;
                detL = scL[si];
                detR = scR != nullptr ? scR[si] : detL;
            }
        }
        else
        {
            detL = out[0][i];
            detR = useCh > 1 ? out[1][i] : detL;
        }

        if (hpf > 8.f)
        {
            const float a = hpfA;
            hpfLpL = a * hpfLpL + (1.f - a) * detL;
            hpfLpR = a * hpfLpR + (1.f - a) * detR;
            detL -= hpfLpL;
            detR -= hpfLpR;
            hpfLpL2 = a * hpfLpL2 + (1.f - a) * detL;
            hpfLpR2 = a * hpfLpR2 + (1.f - a) * detR;
            detL -= hpfLpL2;
            detR -= hpfLpR2;
            if (std::abs (hpfLpL) < 1.0e-20f) hpfLpL = 0.f;
            if (std::abs (hpfLpR) < 1.0e-20f) hpfLpR = 0.f;
            if (std::abs (hpfLpL2) < 1.0e-20f) hpfLpL2 = 0.f;
            if (std::abs (hpfLpR2) < 1.0e-20f) hpfLpR2 = 0.f;
        }

        const float peak = juce::jmax (std::abs (detL), std::abs (detR));
        float level = peak;
        if (rmsDetect)
        {
            const float ms = 0.5f * (detL * detL + detR * detR);
            rmsState += rmsC * (ms - rmsState);
            if (rmsState < 1.0e-20f)
                rmsState = 0.f;
            level = std::sqrt (rmsState);
        }
        const float levelDb = juce::Decibels::gainToDecibels (level, -100.f);
        const float grDb = computeGrDb (levelDb, thr, rat, knee);

        envDb += ((grDb > envDb) ? atkC : relC) * (grDb - envDb);
        if (! std::isfinite (envDb) || std::abs (envDb) < 1.0e-20f)
            envDb = 0.f;
        envDb = juce::jlimit (0.f, 60.f, envDb);

        const float g = juce::Decibels::decibelsToGain (-envDb) * makeupLin;
        const float dryG = 1.f - mix;
        float dryL = out[0][i];
        float dryR = useCh > 1 ? out[1][i] : dryL;
        if (latencySamples >= 2 && delayL != nullptr)
        {
            dryL = DSPUtils::delayRead (delayL, writePos, (float) latencySamples, delayN);
            if (useCh > 1 && delayR != nullptr)
                dryR = DSPUtils::delayRead (delayR, writePos, (float) latencySamples, delayN);
            delayL[writePos] = std::isfinite (out[0][i]) ? out[0][i] : 0.f;
            if (delayR != nullptr)
                delayR[writePos] = useCh > 1 && std::isfinite (out[1][i]) ? out[1][i] : delayL[writePos];
            if (++writePos >= delayN)
                writePos = 0;
        }
        const float wets[2] = { dryL, dryR };
        for (int c = 0; c < useCh; ++c)
        {
            const float wet = DSPUtils::softCeilSample (wets[c] * g, ceilLin);
            out[c][i] = wets[c] * dryG + wet * mix;
        }
    }

    if (! live)
    {
        if (nS > 0)
        {
            thrSm.skip (nS);
            ratioSm.skip (nS);
            atkSm.skip (nS);
            relSm.skip (nS);
            kneeSm.skip (nS);
            makeupSm.skip (nS);
            hpfSm.skip (nS);
            ceilSm.skip (nS);
            mixSm.skip (nS);
        }
    }

    if (yPtr != nullptr)
        *yPtr = buffer.getSample (0, nS - 1);
}
