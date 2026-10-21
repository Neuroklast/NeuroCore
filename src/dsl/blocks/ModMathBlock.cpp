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

void SignalChain::ModMath::clearRuntimeState() noexcept
{
}

void SignalChain::ModMath::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    amountSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    amountSm.setCurrentAndTargetValue (snap (amountExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &amountExpr, &mixExpr });
}

float SignalChain::ModMath::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::ModMath::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    const float amount = juce::jlimit (0.0f, 4.0f, snapEval (amountExpr, 1.0f));
    const float mix    = juce::jlimit (0.0f, 1.0f, snapEval (mixExpr, 1.0f));
    for (int i = 0; i < nS; ++i)
    {
        const float a = (nCh > 0) ? buffer.getReadPointer (0)[i] : 0.0f;
        const float b = (nCh > 1) ? buffer.getReadPointer (1)[i] : 0.0f;

        float math = a + b;
        switch (op)
        {
        case Op::Sub: math = a - b; break;
        case Op::Mul: math = a * b; break;
        case Op::Div: math = (std::fabs (b) > 1e-6f) ? a / b : a; break;
        case Op::Min: math = juce::jmin (a, b); break;
        case Op::Max: math = juce::jmax (a, b); break;
        case Op::Add: break;
        }
        const float result = math * amount;
        const float dryG = 1.0f - mix;
        const float wetG = mix;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            const float dry = w[i];
            w[i] = dry * dryG + result * wetG;
        }
    }
}
