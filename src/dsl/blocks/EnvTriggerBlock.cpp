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

void SignalChain::EnvTrigger::clearRuntimeState() noexcept
{
    triggerActive = false;
    triggerRemaining = 0.0f;
    lastInput = 0.0f;
}

void SignalChain::EnvTrigger::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    thresholdSm.reset (sampleRate, Config::kSmoothingTime);
    holdSm.reset (sampleRate, Config::kSmoothingTime);
    levelSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    thresholdSm.setCurrentAndTargetValue (snap (thresholdExpr, 0.5f));
    holdSm.setCurrentAndTargetValue (snap (holdExpr, 0.1f));
    levelSm.setCurrentAndTargetValue (snap (levelExpr, 1.0f));
    bindings.prepare (varPtr, { &thresholdExpr, &holdExpr, &levelExpr });
    clearRuntimeState();
}

float SignalChain::EnvTrigger::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::EnvTrigger::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    thresholdSm.setTargetValue (snapEval (thresholdExpr, 0.5f));
    holdSm.setTargetValue (snapEval (holdExpr, 0.1f));
    levelSm.setTargetValue (snapEval (levelExpr, 1.0f));

    const float invSr = 1.0f / sampleRate;

    for (int i = 0; i < nS; ++i)
    {
        const float threshold = juce::jlimit (-10.0f, 10.0f, thresholdSm.getNextValue());
        const float holdMs    = juce::jlimit (0.0f, 5000.0f, holdSm.getNextValue());
        const float level     = juce::jlimit (0.0f, 10.0f, levelSm.getNextValue());

        const float in = buffer.getReadPointer (0)[i];

        if (!triggerActive && in > threshold && lastInput <= threshold)
        {
            triggerActive = true;
            triggerRemaining = holdMs * 0.001f / invSr; // samples
        }

        float out = 0.0f;
        if (triggerActive)
        {
            out = level;
            triggerRemaining -= 1.0f;
            if (triggerRemaining <= 0.0f)
                triggerActive = false;
        }

        lastInput = in;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            w[i] = out;
        }
    }
}
