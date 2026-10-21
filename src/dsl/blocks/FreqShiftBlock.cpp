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

void SignalChain::FreqShift::clearRuntimeState() noexcept
{
    processor.reset();
}

void SignalChain::FreqShift::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    HzSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    HzSm.setCurrentAndTargetValue (snap (HzExpr, 0.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &HzExpr, &mixExpr });
    processor.prepare (sampleRate);
}

float SignalChain::FreqShift::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::FreqShift::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    HzSm.setTargetValue (snapEval (HzExpr, 0.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const int useCh = juce::jmin (nCh, 2);
    float* output[2] {};
    for (int ch = 0; ch < useCh; ++ch) output[ch] = buffer.getWritePointer (ch);
    for (int i = 0; i < nS; ++i)
    {
        const float hz = juce::jlimit (-20000.f, 20000.f, HzSm.getNextValue());
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
        float frame[2] {};
        for (int ch = 0; ch < useCh; ++ch) frame[ch] = output[ch][i];
        processor.process (frame, useCh, hz, mix);
        for (int ch = 0; ch < useCh; ++ch) output[ch][i] = frame[ch];
    }
}
