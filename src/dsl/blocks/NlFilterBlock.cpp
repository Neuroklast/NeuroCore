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

void SignalChain::NlFilter::clearRuntimeState() noexcept
{
    z1[0] = 0.0f;
    z1[1] = 0.0f;
    coefficientStep = 0;
    cachedCoefficient = 0.f;
}

void SignalChain::NlFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    cutoffSm.reset (sampleRate, Config::kSmoothingTime);
    driveSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    cutoffSm.setCurrentAndTargetValue (snap (cutoffExpr, 1000.0f));
    driveSm.setCurrentAndTargetValue (snap (driveExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &cutoffExpr, &driveExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::NlFilter::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::NlFilter::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    cutoffSm.setTargetValue (snapEval (cutoffExpr, 1000.0f));
    driveSm.setTargetValue (snapEval (driveExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    const float twoPi = juce::MathConstants<float>::twoPi;

    for (int i = 0; i < nS; ++i)
    {
        const float fcRaw = juce::jlimit (20.0f,
            juce::jmin (20000.0f, sampleRate * 0.45f), cutoffSm.getNextValue());
        // A smoothed control value advances every sample, but the relatively
        // expensive exponential is refreshed at a bounded 16-sample stride.
        if (coefficientStep == 0)
            cachedCoefficient = -std::expm1 (-twoPi * fcRaw / sampleRate);
        coefficientStep = (coefficientStep + 1) & 15;
        const float coeff = cachedCoefficient;
        const float drive = juce::jlimit (0.0f, 10.0f, driveSm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

            const float dryG = 1.0f - mix;
            const float wetG = mix;

            for (int c = 0; c < nCh; ++c)
            {
                float* w = buffer.getWritePointer (c);
                float& z = z1[c % 2];
                float x = w[i];
                x = std::tanh (drive * x);
                z = z + coeff * (x - z);
                w[i] = w[i] * dryG + z * wetG;
            }
    }
}
