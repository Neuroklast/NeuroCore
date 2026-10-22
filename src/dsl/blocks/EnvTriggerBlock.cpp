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
    armed = true;
    rmsState = 0.0f;
    pulseRemaining = 0.0f;
    holdoffRemaining = 0.0f;
    liveTrigger.store (0, std::memory_order_relaxed);
    if (liveTriggerSlot != nullptr)
        liveTriggerSlot->store (0, std::memory_order_relaxed);
}

void SignalChain::EnvTrigger::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    thresholdSm.reset (sampleRate, Config::kSmoothingTime);
    holdSm.reset (sampleRate, Config::kSmoothingTime);
    levelSm.reset (sampleRate, Config::kSmoothingTime);
    sensitivitySm.reset (sampleRate, Config::kSmoothingTime);
    hysteresisSm.reset (sampleRate, Config::kSmoothingTime);
    holdoffSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    thresholdSm.setCurrentAndTargetValue (snap (thresholdExpr, -24.0f));
    holdSm.setCurrentAndTargetValue (snap (holdExpr, 100.0f));
    levelSm.setCurrentAndTargetValue (snap (levelExpr, 1.0f));
    sensitivitySm.setCurrentAndTargetValue (snap (sensitivityExpr, 0.0f));
    hysteresisSm.setCurrentAndTargetValue (snap (hysteresisExpr, 6.0f));
    holdoffSm.setCurrentAndTargetValue (snap (holdoffExpr, 0.0f));
    bindings.prepare (varPtr, { &thresholdExpr, &holdExpr, &levelExpr,
                                &sensitivityExpr, &hysteresisExpr, &holdoffExpr });
    reserveControlLane (spec);
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

    const int laneN = juce::jmin (nS, (int) modLane.size());
    if (laneN <= 0)
        return;

    bindings.refresh();

    thresholdSm.setTargetValue (snapEval (thresholdExpr, -24.0f));
    holdSm.setTargetValue (snapEval (holdExpr, 100.0f));
    levelSm.setTargetValue (snapEval (levelExpr, 1.0f));
    sensitivitySm.setTargetValue (snapEval (sensitivityExpr, 0.0f));
    hysteresisSm.setTargetValue (snapEval (hysteresisExpr, 6.0f));
    holdoffSm.setTargetValue (snapEval (holdoffExpr, 0.0f));

    const float* input = buffer.getReadPointer (0);
    const float rmsCoeff = 1.0f - std::exp (-1.0f / (0.01f * sampleRate));
    int live = 0;

    for (int i = 0; i < laneN; ++i)
    {
        const float threshold = juce::jlimit (-60.0f, 0.0f, thresholdSm.getNextValue());
        const float hysteresis = juce::jlimit (0.0f, 24.0f, hysteresisSm.getNextValue());
        const float sensitivity = juce::jlimit (0.0f, 1.0f, sensitivitySm.getNextValue());
        const float holdMs = juce::jlimit (0.0f, 5000.0f, holdSm.getNextValue());
        const float holdoffMs = juce::jlimit (0.0f, 1000.0f, holdoffSm.getNextValue());
        const float level = juce::jlimit (0.0f, 10.0f, levelSm.getNextValue());

        const float x = input[i];
        float detector;
        if (useRms)
        {
            rmsState += (x * x - rmsState) * rmsCoeff;
            detector = std::sqrt (juce::jmax (0.0f, rmsState));
        }
        else
        {
            detector = std::abs (x);
        }
        detector *= 1.0f + sensitivity * 4.0f;
        const float triggerLevel = std::pow (10.0f, threshold / 20.0f);
        const float releaseLevel = triggerLevel * std::pow (10.0f, -hysteresis / 20.0f);

        if (holdoffRemaining > 0.0f)
            holdoffRemaining = juce::jmax (0.0f, holdoffRemaining - 1.0f);

        if (! armed && detector < releaseLevel)
            armed = true;

        if (armed && holdoffRemaining <= 0.0f && detector > triggerLevel)
        {
            pulseRemaining = juce::jmax (1.0f, holdMs * 0.001f * sampleRate);
            holdoffRemaining = holdoffMs * 0.001f * sampleRate;
            armed = false;
        }

        float out = 0.0f;
        if (pulseRemaining > 0.0f)
        {
            out = level;
            pulseRemaining = juce::jmax (0.0f, pulseRemaining - 1.0f);
        }

        if (out > 0.0f)
            live = 1;
        modLane[(size_t) i] = out;
    }

    liveTrigger.store (live, std::memory_order_relaxed);
    if (liveTriggerSlot != nullptr)
        liveTriggerSlot->store (live, std::memory_order_relaxed);
}
