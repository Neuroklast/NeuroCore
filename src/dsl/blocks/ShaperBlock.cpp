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

void SignalChain::Shaper::clearRuntimeState() noexcept
{
}

float SignalChain::Shaper::foldCurve (float x) noexcept
{
    // Symmetric waveshaper with fold: maps |x| > 1 back into [-1, 1] range.
    const float ax = std::abs (x);
    if (ax <= 1.0f)
        return x;
    // Fold: triangle-wave style fold-back
    const float folded = std::fmod (ax, 2.0f);
    const float foldedFolded = (folded <= 1.0f) ? folded : 2.0f - folded;
    return (x >= 0.0f) ? foldedFolded : -foldedFolded;
}

void SignalChain::Shaper::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    driveSm.reset (sampleRate, Config::kSmoothingTime);
    outputSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    driveSm.setCurrentAndTargetValue (snap (driveExpr, 0.0f));
    outputSm.setCurrentAndTargetValue (snap (outputExpr, 0.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.0f));

    const int modeIdx = (int) juce::jlimit (0.0f, 1.0f, snap (modeExpr, 0.0f));
    mode = static_cast<Mode> (modeIdx);
    bindings.prepare (varPtr, { &driveExpr, &outputExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Shaper::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Shaper::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    // Resolve changes from APVTS/DSL bindings once per callback; the smoother
    // remains sample-continuous, even for host automation and preset restore.
    bindings.refresh();
    driveSm.setTargetValue (juce::jlimit (-24.f, 36.f, snapEval (driveExpr, 0.f)));
    outputSm.setTargetValue (juce::jlimit (-36.f, 12.f, snapEval (outputExpr, 0.f)));
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, snapEval (mixExpr, 1.f)));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float driveDb = juce::jlimit (-24.0f, 36.0f, driveSm.getNextValue());
        const float trimLin = juce::Decibels::decibelsToGain (juce::jlimit (-36.0f, 12.0f, outputSm.getNextValue()));
        const float mix     = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        const float driveLin = juce::Decibels::decibelsToGain (driveDb);

        for (int c = 0; c < useCh; ++c)
        {
            const float dry = out[c][i];
            const float x = dry * driveLin;
            float y = (mode == Mode::Clip) ? juce::jlimit (-1.0f, 1.0f, x) : foldCurve (x);
            y *= trimLin;
            out[c][i] = dry * (1.0f - mix) + y * mix;
        }
    }
}
