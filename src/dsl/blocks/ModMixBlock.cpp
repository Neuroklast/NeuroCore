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

    const int laneN = juce::jmin (nS, (int) modLane.size());
    if (laneN <= 0)
        return;

    bindings.refresh();

    levelASm.setTargetValue (snapEval (levelAExpr, 1.0f));
    levelBSm.setTargetValue (snapEval (levelBExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const Modulator* srcA = inputSources.size() > 0 ? inputSources[0] : nullptr;
    const Modulator* srcB = inputSources.size() > 1 ? inputSources[1] : nullptr;
    const float* slotA = inputSlots.size() > 0 ? inputSlots[0] : nullptr;
    const float* slotB = inputSlots.size() > 1 ? inputSlots[1] : nullptr;

    auto laneAt = [] (const Modulator* source, const float* slot, int index) noexcept
    {
        if (source != nullptr)
            return source->controlAt (index);
        return slot != nullptr ? *slot : 0.0f;
    };

    for (int i = 0; i < laneN; ++i)
    {
        const float levelA = juce::jlimit (0.0f, 4.0f, levelASm.getNextValue());
        const float levelB = juce::jlimit (0.0f, 4.0f, levelBSm.getNextValue());
        const float mix    = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        const float a = laneAt (srcA, slotA, i);
        const float b = laneAt (srcB, slotB, i);
        modLane[(size_t) i] = (a * levelA + b * levelB) * mix;
    }
}
