#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>
#include "../../dsp/SaturationModels.h"

using namespace dsl;

namespace
{
    float snapEval (ExpressionEvaluator& e, float fallback) noexcept
    {
        const float v = e.evaluateLive (0.0f);
        return std::isfinite (v) ? v : fallback;
    }
}

void SignalChain::Saturate::clearRuntimeState() noexcept
{
    processor.reset();
}

float SignalChain::Saturate::tapeCurve (float x) noexcept { return neurokore::saturation::tapeCurve (x); }
float SignalChain::Saturate::tubeCurve (float x) noexcept { return neurokore::saturation::tubeCurve (x); }
float SignalChain::Saturate::transistorCurve (float x) noexcept { return neurokore::saturation::transistorCurve (x); }
float SignalChain::Saturate::transformerCurve (float x) noexcept { return neurokore::saturation::transformerCurve (x); }
float SignalChain::Saturate::diodeCurve (float x) noexcept { return neurokore::saturation::diodeCurve (x); }

void SignalChain::Saturate::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    driveSm.reset (sampleRate, Config::kSmoothingTime);
    dynamicsSm.reset (sampleRate, Config::kSmoothingTime);
    characterSm.reset (sampleRate, Config::kSmoothingTime);
    biasSm.reset (sampleRate, Config::kSmoothingTime);
    toneSm.reset (sampleRate, Config::kSmoothingTime);
    outputSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 36.f, snap (driveExpr, 0.f))));
    biasSm.setCurrentAndTargetValue (snap (biasExpr, 0.0f));
    dynamicsSm.setCurrentAndTargetValue (snap (dynamicsExpr, 0.5f));
    characterSm.setCurrentAndTargetValue (snap (characterExpr, 0.5f));
    toneSm.setCurrentAndTargetValue (snap (toneExpr, 0.5f));
    outputSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-36.f, 12.f, snap (outputExpr, 0.f))));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.0f));

    const int modeIdx = (int) juce::jlimit (0.0f, 4.0f, snap (modeExpr, 1.0f));
    mode = static_cast<Mode> (modeIdx);

    processor.prepare (sampleRate);

    bindings.prepare (varPtr, { &driveExpr, &biasExpr, &dynamicsExpr, &toneExpr, &characterExpr,
                                &outputExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Saturate::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Saturate::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    driveSm.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 36.f, snapEval (driveExpr, 0.f))));
    biasSm.setTargetValue (snapEval (biasExpr, 0.0f));
    dynamicsSm.setTargetValue (snapEval (dynamicsExpr, 0.5f));
    characterSm.setTargetValue (snapEval (characterExpr, 0.5f));
    toneSm.setTargetValue (snapEval (toneExpr, 0.5f));
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (
        juce::jlimit (-36.f, 12.f, snapEval (outputExpr, 0.f))));
    mixSm.setTargetValue (snapEval (mixExpr, 0.0f));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float driveLin = driveSm.getNextValue();
        const float bias    = juce::jlimit (-1.0f, 1.0f, biasSm.getNextValue());
        const float dynamics = juce::jlimit (0.0f, 1.0f, dynamicsSm.getNextValue());
        const float tone    = juce::jlimit (0.0f, 1.0f, toneSm.getNextValue());
        const float trimLin = outputSm.getNextValue();
        const float mix     = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        float frame[2] {};
        for (int c = 0; c < useCh; ++c) frame[c] = out[c][i];
        processor.process (frame, useCh, static_cast<int> (mode), driveLin, bias,
                           dynamics, tone, juce::jlimit (0.f, 1.f, characterSm.getNextValue()),
                           trimLin, mix, autoGain);
        for (int c = 0; c < useCh; ++c) out[c][i] = frame[c];
    }
}
