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
    reserveControlLane (spec);
}

float SignalChain::ModMath::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::ModMath::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nS = buffer.getNumSamples();
    if (nS <= 0)
        return;

    if ((int) modLane.size() < nS)
        modLane.resize ((size_t) nS);

    bindings.refresh();

    const float amount = juce::jlimit (0.0f, 4.0f, snapEval (amountExpr, 1.0f));
    const float mix    = juce::jlimit (0.0f, 1.0f, snapEval (mixExpr, 1.0f));
    const float a = inputSlots.size() > 0 && inputSlots[0] != nullptr ? *inputSlots[0] : 0.0f;
    const float b = inputSlots.size() > 1 && inputSlots[1] != nullptr ? *inputSlots[1] : 0.0f;

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

    const float result = math * amount * mix;
    std::fill (modLane.begin(), modLane.begin() + nS, result);
}
