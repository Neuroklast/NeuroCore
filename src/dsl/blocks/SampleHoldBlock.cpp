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
}

void SignalChain::SampleHold::clearRuntimeState() noexcept
{
    phase = 0.0f;
    held = 0.0f;
}

void SignalChain::SampleHold::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 10.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &rateExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::SampleHold::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::SampleHold::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    rateSm.setTargetValue (snapEval (rateExpr, 10.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const float invSr = 1.0f / sampleRate;

    for (int i = 0; i < nS; ++i)
    {
        const float rate = juce::jlimit (0.0f, 1000.0f, rateSm.getNextValue());
        const float mix  = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        phase += rate * invSr;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            held = (nCh > 0) ? buffer.getReadPointer (0)[i] : 0.0f;
        }

        const float dryG = 1.0f - mix;
        const float wetG = mix;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            const float dry = w[i];
            w[i] = dry * dryG + held * wetG;
        }
    }
}
