#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

namespace
{
    float snapEval (ExpressionEvaluator& e, float fallback) noexcept
    {
        const float v = e.evaluateLive (0.0f);
        return std::isfinite (v) ? v : fallback;
    }

    float rand01 (uint32_t& seed) noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float> (seed >> 8) / 16777216.0f;
    }
}

void SignalChain::Random::clearRuntimeState() noexcept
{
    phase = 0.0f;
    current = 0.0f;
    seed = 12345u;
}

void SignalChain::Random::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    minSm.reset (sampleRate, Config::kSmoothingTime);
    maxSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 10.0f));
    minSm.setCurrentAndTargetValue (snap (minExpr, -1.0f));
    maxSm.setCurrentAndTargetValue (snap (maxExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &rateExpr, &minExpr, &maxExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Random::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Random::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    rateSm.setTargetValue (snapEval (rateExpr, 10.0f));
    minSm.setTargetValue (snapEval (minExpr, -1.0f));
    maxSm.setTargetValue (snapEval (maxExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const float invSr = 1.0f / sampleRate;

    for (int i = 0; i < nS; ++i)
    {
        const float rate = juce::jlimit (0.0f, 1000.0f, rateSm.getNextValue());
        const float lo   = juce::jlimit (-10.0f, 10.0f, minSm.getNextValue());
        const float hi   = juce::jlimit (-10.0f, 10.0f, maxSm.getNextValue());
        const float mix  = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        phase += rate * invSr;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            current = lo + rand01 (seed) * (hi - lo);
        }

        const float dryG = 1.0f - mix;
        const float wetG = mix;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            const float dry = w[i];
            w[i] = dry * dryG + current * wetG;
        }
    }
}
