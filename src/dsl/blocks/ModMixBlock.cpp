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

void SignalChain::ModMix::clearRuntimeState() noexcept
{
}

void SignalChain::ModMix::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    levelASm.reset (sampleRate, Config::kSmoothingTime);
    levelBSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    levelASm.setCurrentAndTargetValue (snap (levelAExpr, 1.0f));
    levelBSm.setCurrentAndTargetValue (snap (levelBExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &levelAExpr, &levelBExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::ModMix::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::ModMix::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    levelASm.setTargetValue (snapEval (levelAExpr, 1.0f));
    levelBSm.setTargetValue (snapEval (levelBExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    for (int i = 0; i < nS; ++i)
    {
        const float levelA = juce::jlimit (0.0f, 4.0f, levelASm.getNextValue());
        const float levelB = juce::jlimit (0.0f, 4.0f, levelBSm.getNextValue());
        const float mix    = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        const float a = (nCh > 0) ? buffer.getReadPointer (0)[i] : 0.0f;
        const float b = (nCh > 1) ? buffer.getReadPointer (1)[i] : 0.0f;

        const float wet = a * levelA + b * levelB;
        const float dryG = 1.0f - mix;
        const float wetG = mix;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            const float dry = w[i];
            w[i] = dry * dryG + wet * wetG;
        }
    }
}
