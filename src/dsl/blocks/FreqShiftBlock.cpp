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
    ap0L = AP1{}; ap1L = AP1{};
    ap0R = AP1{}; ap1R = AP1{};
    phase = 0.0f;
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
    clearRuntimeState();
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

    const float srInv = 1.0f / sampleRate;

    auto allpassSet = [this] (AP1& ap, float freq) noexcept
    {
        const float w = 2.f * juce::MathConstants<float>::pi *
                        juce::jlimit (1.f, sampleRate * 0.49f, std::abs (freq)) / sampleRate;
        ap.a = std::tan (w * 0.5f);
        ap.a = juce::jlimit (-0.99f, 0.99f, ap.a);
    };
    auto allpassPush = [] (AP1& ap, float x) noexcept -> float
    {
        const float y = ap.a * (x - ap.z1) + ap.z1;
        ap.z1 = x;
        return y;
    };

    for (int i = 0; i < nS; ++i)
    {
        const float hz = juce::jlimit (-20000.0f, 20000.0f, HzSm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());
        const float omega = hz * juce::MathConstants<float>::twoPi * srInv;

        phase += omega;
        while (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        const float c = std::cos (phase);
        const float s = std::sin (phase);

        allpassSet (ap0L, hz); allpassSet (ap1L, hz);
        allpassSet (ap0R, hz); allpassSet (ap1R, hz);

        const float dryG = 1.0f - mix;
        const float wetG = mix;

        if (nCh >= 1)
        {
            float* w = buffer.getWritePointer (0);
            const float x0 = w[i];
            const float iPath = allpassPush (ap0L, x0);
            const float qPath = allpassPush (ap1L, x0);
            w[i] = x0 * dryG + (iPath * c - qPath * s) * wetG;
        }
        if (nCh >= 2)
        {
            float* w = buffer.getWritePointer (1);
            const float x1 = w[i];
            const float iPath = allpassPush (ap0R, x1);
            const float qPath = allpassPush (ap1R, x1);
            w[i] = x1 * dryG + (iPath * c - qPath * s) * wetG;
        }
    }
}
