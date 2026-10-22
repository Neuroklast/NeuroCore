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
    slewValue = 0.0f;
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
    bindings.prepare (varPtr, { &amountExpr, &mixExpr, &clampMinExpr, &clampMaxExpr,
                                &inMinExpr, &inMaxExpr, &outMinExpr, &outMaxExpr,
                                &quantizeExpr, &slewExpr });
    reserveControlLane (spec);
    clearRuntimeState();
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

    const int laneN = juce::jmin (nS, (int) modLane.size());
    if (laneN <= 0)
        return;

    bindings.refresh();

    amountSm.setTargetValue (snapEval (amountExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));
    const float clampLo = juce::jmin (snapEval (clampMinExpr, -1.0f),
                                      snapEval (clampMaxExpr, 1.0f));
    const float clampHi = juce::jmax (snapEval (clampMinExpr, -1.0f),
                                      snapEval (clampMaxExpr, 1.0f));
    const float inLo = snapEval (inMinExpr, -1.0f);
    const float inHi = snapEval (inMaxExpr, 1.0f);
    const float outLo = snapEval (outMinExpr, -1.0f);
    const float outHi = snapEval (outMaxExpr, 1.0f);
    const float quantize = juce::jlimit (0.0f, 1.0f, snapEval (quantizeExpr, 1.0f));
    const int levels = juce::jmax (2, (int) std::lround (2.0 + (1.0 - quantize) * 62.0));
    const float quantStep = 2.0f / (float) (levels - 1);
    const float smooth = juce::jlimit (0.0f, 1.0f, snapEval (slewExpr, 0.0f));
    const float slewCoeff = smooth <= 1.0e-4f
                                ? 1.0f
                                : 1.0f - std::exp (-1.0f / (juce::jmax (0.001f, smooth * 0.5f) * sampleRate));

    const Modulator* srcA = inputSources.size() > 0 ? inputSources[0] : nullptr;
    const Modulator* srcB = inputSources.size() > 1 ? inputSources[1] : nullptr;
    const float* slotA = inputSlots.size() > 0 ? inputSlots[0] : nullptr;
    const float* slotB = inputSlots.size() > 1 ? inputSlots[1] : nullptr;
    const float span = inHi - inLo;

    auto laneAt = [] (const Modulator* source, const float* slot, int index) noexcept
    {
        if (source != nullptr)
            return source->controlAt (index);
        return slot != nullptr ? *slot : 0.0f;
    };

    for (int i = 0; i < laneN; ++i)
    {
        const float amount = juce::jlimit (0.0f, 4.0f, amountSm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());
        const float a = laneAt (srcA, slotA, i);
        const float b = laneAt (srcB, slotB, i);
        float result = 0.0f;

        switch (op)
        {
        case Op::Add:    result = a + b; break;
        case Op::Sub:    result = a - b; break;
        case Op::Mul:    result = a * b; break;
        case Op::Div:    result = std::fabs (b) > 1.0e-6f ? a / b : 0.0f; break;
        case Op::Min:    result = juce::jmin (a, b); break;
        case Op::Max:    result = juce::jmax (a, b); break;
        case Op::Abs:    result = std::abs (a); break;
        case Op::Invert: result = -a; break;
        case Op::Clamp:  result = juce::jlimit (clampLo, clampHi, a); break;
        case Op::Remap:
            result = std::abs (span) < 1.0e-6f
                         ? outLo
                         : outLo + (a - inLo) * (outHi - outLo) / span;
            break;
        case Op::Quantize:
            result = juce::jlimit (-1.0f, 1.0f, std::round (a / quantStep) * quantStep);
            break;
        case Op::Slew:
            slewValue += (a - slewValue) * slewCoeff;
            result = slewValue;
            break;
        default:
            break;
        }

        if (op != Op::Clamp && op != Op::Remap && op != Op::Quantize && op != Op::Slew)
            result *= amount;

        modLane[(size_t) i] = result * mix;
    }
}
