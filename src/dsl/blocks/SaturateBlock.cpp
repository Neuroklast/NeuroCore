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

void SignalChain::Saturate::clearRuntimeState() noexcept
{
    peakEnv = 0.0f;
    autoGainSm = 1.0f;
}

float SignalChain::Saturate::tapeCurve (float x) noexcept
{
    // Tape: mild saturation with slight asymmetry and gentle HF rolloff character.
    const float ax = std::abs (x);
    if (ax <= 0.7f)
        return x * (1.0f - 0.15f * ax);
    return (x >= 0.0f) ? 0.7f + 0.3f * std::tanh ((ax - 0.7f) / 0.3f)
                       : -0.7f - 0.3f * std::tanh ((ax - 0.7f) / 0.3f);
}

float SignalChain::Saturate::tubeCurve (float x) noexcept
{
    // Tube: asymmetric soft saturation, even-harmonic character.
    const float ax = std::abs (x);
    if (ax <= 0.6f)
        return x;
    const float y = (x >= 0.0f) ? (1.0f - std::exp (-ax / 0.6f)) : -(1.0f - std::exp (-ax / 0.6f));
    return juce::jlimit (-1.0f, 1.0f, y * 1.2f);
}

float SignalChain::Saturate::transistorCurve (float x) noexcept
{
    // Transistor: harder saturation, more odd harmonics.
    const float ax = std::abs (x);
    if (ax <= 0.5f)
        return x;
    const float y = std::tanh (ax / 0.5f);
    return (x >= 0.0f) ? juce::jlimit (0.0f, 1.0f, y) : -juce::jlimit (0.0f, 1.0f, y);
}

float SignalChain::Saturate::transformerCurve (float x) noexcept
{
    // Transformer: LF saturation character with HF rolloff, gentle.
    const float ax = std::abs (x);
    if (ax <= 0.8f)
        return x * 0.95f;
    return (x >= 0.0f) ? 0.76f + 0.24f * std::tanh ((ax - 0.8f) / 0.2f)
                       : -0.76f - 0.24f * std::tanh ((ax - 0.8f) / 0.2f);
}

float SignalChain::Saturate::diodeCurve (float x) noexcept
{
    // Diode: hard asymmetric clipping.
    const float ax = std::abs (x);
    if (ax <= 0.6f)
        return x;
    return (x >= 0.0f) ? juce::jlimit (0.0f, 1.0f, ax / 0.6f) : -juce::jlimit (0.0f, 1.0f, ax / 0.6f);
}

void SignalChain::Saturate::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    driveSm.reset (sampleRate, Config::kSmoothingTime);
    biasSm.reset (sampleRate, Config::kSmoothingTime);
    toneSm.reset (sampleRate, Config::kSmoothingTime);
    outputSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    driveSm.setCurrentAndTargetValue (snap (driveExpr, 0.0f));
    biasSm.setCurrentAndTargetValue (snap (biasExpr, 0.0f));
    dynamicsSm.setCurrentAndTargetValue (snap (dynamicsExpr, 0.5f));
    toneSm.setCurrentAndTargetValue (snap (toneExpr, 0.5f));
    outputSm.setCurrentAndTargetValue (snap (outputExpr, 0.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.0f));

    const int modeIdx = (int) juce::jlimit (0.0f, 4.0f, snap (modeExpr, 1.0f));
    mode = static_cast<Mode> (modeIdx);

    const float atkS = 0.005f;
    const float relS = 0.100f;
    agcAtkC = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, atkS * sampleRate));
    agcRelC = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, relS * sampleRate));

    bindings.prepare (varPtr, { &driveExpr, &biasExpr, &dynamicsExpr, &toneExpr,
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

    driveSm.setTargetValue (snapEval (driveExpr, 0.0f));
    biasSm.setTargetValue (snapEval (biasExpr, 0.0f));
    dynamicsSm.setTargetValue (snapEval (dynamicsExpr, 0.5f));
    toneSm.setTargetValue (snapEval (toneExpr, 0.5f));
    outputSm.setTargetValue (snapEval (outputExpr, 0.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 0.0f));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float driveDb = juce::jlimit (-24.0f, 36.0f, driveSm.getNextValue());
        const float bias    = juce::jlimit (-1.0f, 1.0f, biasSm.getNextValue());
        const float dynamics = juce::jlimit (0.0f, 1.0f, dynamicsSm.getNextValue());
        const float tone    = juce::jlimit (0.0f, 1.0f, toneSm.getNextValue());
        const float trimLin = juce::Decibels::decibelsToGain (juce::jlimit (-36.0f, 12.0f, outputSm.getNextValue()));
        const float mix     = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        const float driveLin = juce::Decibels::decibelsToGain (driveDb);

        float inPeak = 0.0f;
        for (int c = 0; c < useCh; ++c)
            inPeak = juce::jmax (inPeak, std::abs (out[c][i]));
        if (! std::isfinite (inPeak))
            inPeak = 0.0f;

        if (inPeak > peakEnv)
            peakEnv += (inPeak - peakEnv) * agcAtkC;
        else
            peakEnv += (inPeak - peakEnv) * agcRelC;
        if (! std::isfinite (peakEnv) || peakEnv < 0.0f)
            peakEnv = 0.0f;

        float agcTarget = 1.0f;
        if (autoGain && peakEnv > 0.5f)
            agcTarget = 0.5f / juce::jmax (1.0e-4f, peakEnv);
        if (agcTarget < autoGainSm)
            autoGainSm += (agcTarget - autoGainSm) * agcAtkC;
        else
            autoGainSm += (agcTarget - autoGainSm) * agcRelC;
        if (! std::isfinite (autoGainSm))
            autoGainSm = 1.0f;

        const float effDrive = driveLin * autoGainSm;

        for (int c = 0; c < useCh; ++c)
        {
            float dry = out[c][i];
            float x = dry * effDrive;
            x += bias;
            float y;
            switch (mode)
            {
                case Mode::Tape:    y = tapeCurve (x); break;
                case Mode::Tube:    y = tubeCurve (x); break;
                case Mode::Transistor: y = transistorCurve (x); break;
                case Mode::Transformer: y = transformerCurve (x); break;
                case Mode::Diode:   y = diodeCurve (x); break;
                default:            y = tubeCurve (x); break;
            }
            y *= trimLin;
            out[c][i] = dry * (1.0f - mix) + y * mix;
        }
    }
}
