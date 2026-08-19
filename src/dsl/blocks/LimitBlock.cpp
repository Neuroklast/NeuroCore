#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Limit::clearRuntimeState() noexcept
{
    gain = 1.f;
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
    // ~80 µs attack — instant slam clicked; hard clip still holds the ceiling
    atkC = 1.f - std::exp (-1.f / juce::jmax (1.f, 0.00008f * sampleRate));
    clearRuntimeState();
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

float SignalChain::Limit::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::AudioBuffer<float> one (1, 1);
    one.setSample (0, 0, x);
    processBlock (one);
    return one.getSample (0, 0);
}

void SignalChain::Limit::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    for (const auto& n : varNames)
    {
        const float v = *n.first;
        ceilingDb.setVariable (n.second, v);
        release.setVariable (n.second, v);
    }

    auto ev = [] (ExpressionEvaluator& e, float fallback)
    {
        const float v = e.evaluate (0.f);
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

        float peak = 0.f;
        for (int c = 0; c < useCh; ++c)
            peak = juce::jmax (peak, std::abs (out[c][i]));

        const float needed = (peak > ceilLin && peak > 1.0e-12f) ? (ceilLin / peak) : 1.f;
        if (needed < gain)
            gain += atkC * (needed - gain);
        else
            gain += relC * (1.f - gain);
        if (! std::isfinite (gain) || std::abs (gain) < 1.0e-20f)
            gain = (needed < 1.f) ? needed : 1.f;
        gain = juce::jlimit (0.f, 1.f, gain);

        for (int c = 0; c < useCh; ++c)
        {
            float y = out[c][i] * gain;
            if (! std::isfinite (y))
                y = 0.f;
            else if (std::abs (y) > ceilLin)
                y = std::copysign (ceilLin, y);
            out[c][i] = y;
        }
    }
}
