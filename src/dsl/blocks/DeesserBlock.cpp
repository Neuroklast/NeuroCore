#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <cmath>

using namespace dsl;

void SignalChain::Deesser::clearRuntimeState() noexcept
{
    broadE = bandE = gr = 0.f;
    cachedAtk = cachedRel = -1.f;
    cachedFreq = -1.f;
    band.reset();
}

void SignalChain::Deesser::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    band.prepare (spec);
    band.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    band.setResonance (1.0f);
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (freqSm, freqExpr, 6500.f);
    snap (thrSm, thresholdExpr, -8.f);
    snap (amtSm, amountExpr, 0.7f);
    snap (atkSm, attackExpr, 0.002f);
    snap (relSm, releaseExpr, 0.04f);
    snap (mixSm, mixExpr, 1.f);
    detC = 1.f - std::exp (-1.f / (0.004f * sampleRate));
    bindings.prepare (varPtr, { &freqExpr, &thresholdExpr, &amountExpr, &attackExpr, &releaseExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Deesser::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Deesser::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();
    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluateLive (0.f);
        return std::isfinite (v) ? v : fb;
    };
    freqSm.setTargetValue (juce::jlimit (2000.f, 12000.f, ev (freqExpr, 6500.f)));
    thrSm.setTargetValue (juce::jlimit (-24.f, 6.f, ev (thresholdExpr, -8.f)));
    amtSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (amountExpr, 0.7f)));
    atkSm.setTargetValue (juce::jmax (0.0002f, ev (attackExpr, 0.002f)));
    relSm.setTargetValue (juce::jmax (0.005f, ev (releaseExpr, 0.04f)));
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (mixExpr, 1.f)));

    float* L = buffer.getWritePointer (0);
    float* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float ny = sampleRate * 0.45f;

    for (int i = 0; i < nS; ++i)
    {
        const float freq = juce::jlimit (2000.f, juce::jmin (12000.f, ny), freqSm.getNextValue());
        const float thr = thrSm.getNextValue();
        const float amount = amtSm.getNextValue();
        const float atk = atkSm.getNextValue();
        const float rel = relSm.getNextValue();
        const float mix = mixSm.getNextValue();
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
        if (std::abs (freq - cachedFreq) > 1.f)
        {
            cachedFreq = freq;
            band.setCutoffFrequency (freq);
        }

        const float xL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float xR = R != nullptr && std::isfinite (R[i]) ? R[i] : xL;
        const float bL = band.processSample (0, xL);
        const float bR = nCh > 1 ? band.processSample (1, xR) : bL;
        const float broad = juce::jmax (std::abs (xL), std::abs (xR));
        const float bandAbs = juce::jmax (std::abs (bL), std::abs (bR));
        broadE += detC * (broad - broadE);
        bandE += detC * (bandAbs - bandE);
        if (broadE < 1.0e-20f) broadE = 0.f;
        if (bandE < 1.0e-20f) bandE = 0.f;

        float target = 0.f;
        if (broadE > 1.0e-4f)
        {
            const float relDb = juce::Decibels::gainToDecibels (bandE, -100.f)
                              - juce::Decibels::gainToDecibels (broadE, -100.f);
            if (relDb > thr)
                target = 1.f;
        }
        gr += ((target > gr) ? atkC : relC) * (target - gr);
        if (! std::isfinite (gr) || gr < 1.0e-20f)
            gr = 0.f;
        gr = juce::jlimit (0.f, 1.f, gr);
        const float bandGain = 1.f - amount * gr;

        auto emit = [&] (float x, float b, int ch)
        {
            float wet = listen ? b : ((x - b) + b * bandGain);
            if (! std::isfinite (wet))
                wet = 0.f;
            const float y = x * (1.f - mix) + wet * mix;
            if (ch == 0)
                L[i] = y;
            else if (R != nullptr)
                R[i] = y;
        };
        emit (xL, bL, 0);
        if (nCh > 1)
            emit (xR, bR, 1);
    }
}
