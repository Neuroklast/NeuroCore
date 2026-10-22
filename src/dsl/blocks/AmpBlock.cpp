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

void SignalChain::Amp::clearRuntimeState() noexcept
{
    // Parameter smoothers intentionally survive script runtime resets.
}

void SignalChain::Amp::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    gainSm.reset (sampleRate, Config::kSmoothingTime);
    trimSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 24.f, snap (gainExpr, 0.f))));
    trimSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-36.f, 12.f, snap (outputExpr, 0.f))));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.0f));
    bindings.prepare (varPtr, { &gainExpr, &outputExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Amp::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Amp::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 24.f, snapEval (gainExpr, 0.f))));
    trimSm.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-36.f, 12.f, snapEval (outputExpr, 0.f))));
    mixSm.setTargetValue (snapEval (mixExpr, 0.0f));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float gain = gainSm.getNextValue();
        const float trim = trimSm.getNextValue();
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
        const float g = gain * trim;

        for (int c = 0; c < useCh; ++c)
        {
            const float dry = out[c][i];
            const float y = dry * g;
            out[c][i] = dry * (1.0f - mix) + y * mix;
        }
    }
}
