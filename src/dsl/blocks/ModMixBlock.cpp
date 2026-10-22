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
    reserveControlLane (spec);
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
    const int nS = buffer.getNumSamples();
    if (nS <= 0)
        return;

    if ((int) modLane.size() < nS)
        modLane.resize ((size_t) nS);

    bindings.refresh();

    levelASm.setTargetValue (snapEval (levelAExpr, 1.0f));
    levelBSm.setTargetValue (snapEval (levelBExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const float a = inputSlots.size() > 0 && inputSlots[0] != nullptr ? *inputSlots[0] : 0.0f;
    const float b = inputSlots.size() > 1 && inputSlots[1] != nullptr ? *inputSlots[1] : 0.0f;

    for (int i = 0; i < nS; ++i)
    {
        const float levelA = juce::jlimit (0.0f, 4.0f, levelASm.getNextValue());
        const float levelB = juce::jlimit (0.0f, 4.0f, levelBSm.getNextValue());
        const float mix    = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        modLane[(size_t) i] = (a * levelA + b * levelB) * mix;
    }
}
