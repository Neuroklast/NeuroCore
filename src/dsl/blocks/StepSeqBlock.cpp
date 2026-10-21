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

void SignalChain::StepSeq::clearRuntimeState() noexcept
{
    phase = 0.0f;
    stepIndex = 0;
}

void SignalChain::StepSeq::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    for (int i = 0; i < 4; ++i)
        stepSm[i].reset (sampleRate, Config::kSmoothingTime);
    for (int i = 0; i < 4; ++i)
        stepSm[i].setCurrentAndTargetValue (snap (stepExpr[i], 0.0f));
    bindings.prepare (varPtr, { &rateExpr, &mixExpr, &stepExpr[0], &stepExpr[1], &stepExpr[2], &stepExpr[3] });
    clearRuntimeState();
}

float SignalChain::StepSeq::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::StepSeq::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    rateSm.setTargetValue (snapEval (rateExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));
    for (int i = 0; i < 4; ++i)
        stepSm[i].setTargetValue (snapEval (stepExpr[i], 0.0f));

    const float numSteps = 4.f;
    const float invSr = 1.0f / sampleRate;

    for (int i = 0; i < nS; ++i)
    {
        const float rate = juce::jlimit (0.0f, 100.0f, rateSm.getNextValue());
        const float mix  = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        phase += rate * invSr;
        while (phase >= 1.0f)
        {
            phase -= 1.0f;
            stepIndex = (stepIndex + 1) % 4;
        }

        const float cur = stepSm[stepIndex].getNextValue();

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            w[i] = cur * mix; // stepseq replaces input (mod source)
        }
    }
}
