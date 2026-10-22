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

void SignalChain::Resonator::clearRuntimeState() noexcept
{
    filter.reset();
    cachedF = -1.f;
    cachedQ = -1.f;
    coefficientStep = 0;
}

void SignalChain::Resonator::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    freqSm.reset (sampleRate, Config::kSmoothingTime);
    resSm.reset (sampleRate, Config::kSmoothingTime);
    gainSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    freqSm.setCurrentAndTargetValue (snap (freqExpr, 1000.0f));
    resSm.setCurrentAndTargetValue (snap (resExpr, 1.0f));
    gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 24.f, snap (gainExpr, 0.f))));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));

    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    bindings.prepare (varPtr, { &freqExpr, &resExpr, &gainExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Resonator::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Resonator::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    freqSm.setTargetValue (snapEval (freqExpr, 1000.0f));
    resSm.setTargetValue (snapEval (resExpr, 1.0f));
    gainSm.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 24.f, snapEval (gainExpr, 0.f))));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float f = juce::jlimit (20.f,
            juce::jmin (20000.f, sampleRate * 0.45f), freqSm.getNextValue());
        const float q = juce::jlimit (0.1f, 10.f, resSm.getNextValue());
        const float gain = gainSm.getNextValue();
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());

        // The SVF runs at audio rate; only its expensive coefficient
        // calculations are refreshed every 16 samples. A smoothing value
        // still advances on each frame and no heap allocations occur here.
        if (coefficientStep == 0 &&
            (std::abs (f - cachedF) > 1.0e-3f || std::abs (q - cachedQ) > 1.0e-3f))
        {
            cachedF = f;
            cachedQ = q;
            filter.setCutoffFrequency (f);
            filter.setResonance (q);
        }
        coefficientStep = (coefficientStep + 1) & 15;

        const float g = gain * mix;

        for (int c = 0; c < useCh; ++c)
        {
            const float dry = out[c][i];
            const float y = filter.processSample (c, dry) * g;
            out[c][i] = dry * (1.0f - mix) + y;
        }
    }
}
