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

    float foldTriangle (float x, float fold) noexcept
    {
        const float period = 4.0f * fold;
        float t = std::fmod (x + fold, period);
        if (t < 0.0f) t += period;
        return fold - std::abs (t - 2.0f * fold);
    }
}

void SignalChain::WaveFold::clearRuntimeState() noexcept {}

void SignalChain::WaveFold::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    driveSm.reset (sampleRate, Config::kSmoothingTime);
    foldSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    driveSm.setCurrentAndTargetValue (snap (driveExpr, 1.0f));
    foldSm.setCurrentAndTargetValue (snap (foldExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &driveExpr, &foldExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::WaveFold::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::WaveFold::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    driveSm.setTargetValue (snapEval (driveExpr, 1.0f));
    foldSm.setTargetValue (snapEval (foldExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));

    for (int i = 0; i < nS; ++i)
    {
        const float drive = juce::jlimit (0.0f, 100.0f, driveSm.getNextValue());
        const float fold  = juce::jlimit (0.001f, 4.0f, foldSm.getNextValue());
        const float mix   = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        const float dryG = 1.0f - mix;
        const float wetG = mix;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            const float x = foldTriangle (w[i] * drive, fold);
            w[i] = w[i] * dryG + x * wetG;
        }
    }
}
