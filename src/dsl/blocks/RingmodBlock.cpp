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

void SignalChain::Ringmod::clearRuntimeState() noexcept
{
    phase = 0.f;
}

void SignalChain::Ringmod::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    freqSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    freqSm.setCurrentAndTargetValue (snap (freqExpr, 440.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &freqExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Ringmod::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Ringmod::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    freqSm.setTargetValue (snapEval (freqExpr, 440.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    const float invSr = 1.0f / sampleRate;

    for (int i = 0; i < nS; ++i)
    {
        const float freq = juce::jlimit (20.0f, 2000.0f, freqSm.getNextValue());
        const float mix  = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        phase += freq * invSr;
        if (phase >= 1.0f)
            phase -= 1.0f;

        const float carrier = std::sin (juce::MathConstants<float>::twoPi * phase);

        for (int c = 0; c < useCh; ++c)
        {
            const float dry = out[c][i];
            const float y = dry * carrier;
            out[c][i] = dry * (1.0f - mix) + y * mix;
        }
    }
}
