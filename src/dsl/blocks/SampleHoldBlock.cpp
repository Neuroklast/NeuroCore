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

    float quantizeValue (float v, float amount) noexcept
    {
        if (amount <= 1.0e-4f)
            return v;
        const int levels = juce::jmax (2, (int) std::lround (2.0 + (1.0 - amount) * 62.0));
        const float step = 2.0f / (float) (levels - 1);
        return juce::jlimit (-1.0f, 1.0f, std::round (v / step) * step);
    }
}

void SignalChain::SampleHold::clearRuntimeState() noexcept
{
    phase = 0.0;
    held = 0.0f;
    slewValue = 0.0f;
    holdRemaining = 0.0f;
}

void SignalChain::SampleHold::applyTempo (double bpm) noexcept
{
    if (bpm > 0.0)
        hostBpm = bpm;
}

void SignalChain::SampleHold::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    holdSm.reset (sampleRate, Config::kSmoothingTime);
    slewSm.reset (sampleRate, Config::kSmoothingTime);
    quantizeSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 10.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    holdSm.setCurrentAndTargetValue (snap (holdExpr, 0.0f));
    slewSm.setCurrentAndTargetValue (snap (slewExpr, 0.0f));
    quantizeSm.setCurrentAndTargetValue (snap (quantizeExpr, 0.0f));
    bindings.prepare (varPtr, { &rateExpr, &mixExpr, &holdExpr, &slewExpr, &quantizeExpr });
    reserveControlLane (spec);
    clearRuntimeState();
}

float SignalChain::SampleHold::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::SampleHold::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    const int laneN = juce::jmin (nS, (int) modLane.size());
    if (laneN <= 0)
        return;

    bindings.refresh();

    rateSm.setTargetValue (snapEval (rateExpr, 10.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));
    holdSm.setTargetValue (snapEval (holdExpr, 0.0f));
    slewSm.setTargetValue (snapEval (slewExpr, 0.0f));
    quantizeSm.setTargetValue (snapEval (quantizeExpr, 0.0f));

    const double invSr = 1.0 / (double) sampleRate;
    const double bpm = hostBpm > 0.0 ? hostBpm : (double) Config::kDefaultTempo;
    const double syncHz = syncEnabled
                              ? bpm / (juce::jmax (1.0 / 64.0, (double) syncWhole) * 240.0)
                              : 0.0;
    const float* audioIn = buffer.getReadPointer (0);
    const float* controlIn = inputSlots.empty() ? nullptr : inputSlots.front();
    const Modulator* controlSource = inputSources.empty() ? nullptr : inputSources.front();

    for (int i = 0; i < laneN; ++i)
    {
        const float rate = juce::jlimit (0.0f, 1000.0f, rateSm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());
        const float hold = juce::jlimit (0.0f, 5.0f, holdSm.getNextValue());
        const float slew = juce::jlimit (0.0f, 1.0f, slewSm.getNextValue());
        const float quantize = juce::jlimit (0.0f, 1.0f, quantizeSm.getNextValue());

        if (holdRemaining > 0.0f)
            holdRemaining = juce::jmax (0.0f, holdRemaining - 1.0f);

        const double stepHz = syncEnabled ? syncHz : (double) rate;
        if (stepHz > 0.0)
            phase += stepHz * invSr;

        while (phase >= 1.0)
        {
            phase -= 1.0;
            if (holdRemaining <= 0.0f)
            {
                float input = audioIn[i];
                if (controlSource != nullptr)
                    input = controlSource->controlAt (i);
                else if (controlIn != nullptr)
                    input = *controlIn;
                held = quantizeValue (input, quantize);
                holdRemaining = hold * sampleRate;
            }
        }

        const float target = held * mix;
        if (slew <= 1.0e-4f)
            slewValue = target;
        else
        {
            const float tc = juce::jmax (0.001f, slew * 0.5f);
            const float coeff = 1.0f - std::exp (-1.0f / (tc * sampleRate));
            slewValue += (target - slewValue) * coeff;
        }

        modLane[(size_t) i] = slewValue;
    }
}
