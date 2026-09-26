#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <cmath>

using namespace dsl;

void SignalChain::Limit::clearRuntimeState() noexcept
{
    gain = 1.f;
    writePos = 0;
    cachedRel = -1.f;
    cachedCeil = 1.0e9f;
    if (delayL != nullptr && delayN > 0)
        std::fill (delayL, delayL + delayN, 0.f);
    if (delayR != nullptr && delayN > 0)
        std::fill (delayR, delayR + delayN, 0.f);
}

void SignalChain::Limit::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    ceilSm.reset (sampleRate, Config::kSmoothingTime);
    relSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fallback;
    };
    ceilSm.setCurrentAndTargetValue (snap (ceilingDb, -0.3f));
    relSm.setCurrentAndTargetValue (snap (release, 0.08f));
    float aheadMs = snap (lookaheadMs, 0.f);
    aheadMs = juce::jlimit (0.f, kMaxLookaheadSec * 1000.f, aheadMs);
    const int ahead = (int) std::lround ((double) aheadMs * 0.001 * (double) sampleRate);
    delayN = juce::jmax (8, (int) std::ceil ((double) sampleRate * (double) kMaxLookaheadSec) + 4);
    delayL = DSPUtils::alignedRing (storageL, delayN);
    delayR = DSPUtils::alignedRing (storageR, delayN);
    latencySamples = (ahead >= 2 && ahead <= delayN - 2) ? ahead : 0;
    // ~80 µs attack — instant slam clicked; hard clip still holds the ceiling
    atkC = 1.f - std::exp (-1.f / juce::jmax (1.f, 0.00008f * sampleRate));
    clearRuntimeState();
    bindings.prepare (varPtr, { &ceilingDb, &release, &lookaheadMs });
}

float SignalChain::Limit::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    float* data[] { &x };
    juce::AudioBuffer<float> one (data, 1, 1);
    processBlock (one);
    return one.getSample (0, 0);
}

void SignalChain::Limit::processBlock (juce::AudioBuffer<float>& buffer)
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
    ceilSm.setTargetValue (ev (ceilingDb, -0.3f));
    relSm.setTargetValue (ev (release, 0.08f));

    float* out[2] {};
    const int useCh = juce::jmin (nCh, 2);
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float ceilDb = juce::jlimit (-24.f, 0.f, ceilSm.getNextValue());
        const float relS   = juce::jlimit (0.01f, 1.f, relSm.getNextValue());
        if (std::abs (ceilDb - cachedCeil) > 1.0e-4f)
        {
            cachedCeil = ceilDb;
            ceilLin = juce::Decibels::decibelsToGain (ceilDb);
        }
        if (std::abs (relS - cachedRel) > 1.0e-6f)
        {
            cachedRel = relS;
            relC = 1.f - std::exp (-1.f / (relS * sampleRate));
        }

        float inL = std::isfinite (out[0][i]) ? out[0][i] : 0.f;
        float inR = useCh > 1 ? (std::isfinite (out[1][i]) ? out[1][i] : 0.f) : inL;
        const float peak = juce::jmax (std::abs (inL), std::abs (inR));
        const float needed = (peak > ceilLin && peak > 1.0e-12f) ? (ceilLin / peak) : 1.f;
        if (latencySamples >= 2 && delayL != nullptr)
        {
            // Future peak sets the gain now. The delayed sample exits already reduced.
            if (needed < gain)
                gain = needed;
            else
                gain += relC * (1.f - gain);
        }
        else if (needed < gain)
            gain += atkC * (needed - gain);
        else
            gain += relC * (1.f - gain);
        if (! std::isfinite (gain) || std::abs (gain) < 1.0e-20f)
            gain = (needed < 1.f) ? needed : 1.f;
        gain = juce::jlimit (0.f, 1.f, gain);

        float yL = inL;
        float yR = inR;
        if (latencySamples >= 2 && delayL != nullptr)
        {
            yL = DSPUtils::delayRead (delayL, writePos, (float) latencySamples, delayN);
            if (useCh > 1 && delayR != nullptr)
                yR = DSPUtils::delayRead (delayR, writePos, (float) latencySamples, delayN);
            delayL[writePos] = inL;
            if (delayR != nullptr)
                delayR[writePos] = inR;
            if (++writePos >= delayN)
                writePos = 0;
        }

        auto hold = [ceilLin = ceilLin, gain = gain] (float x)
        {
            float y = x * gain;
            if (! std::isfinite (y))
                return 0.f;
            if (std::abs (y) > ceilLin)
                y = std::copysign (ceilLin, y);
            return y;
        };
        out[0][i] = hold (yL);
        if (useCh > 1)
            out[1][i] = hold (yR);
    }
}
