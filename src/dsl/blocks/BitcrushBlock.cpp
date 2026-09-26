#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Bitcrush::clearRuntimeState() noexcept
{
    zL = zR = 0.f;
}

void SignalChain::Bitcrush::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (bitsSm, bitsExpr, 8.f);
    snap (mixSm, mixExpr, 1.f);
    snap (toneSm, toneExpr, 8000.f);
    const float tone = juce::jlimit (800.f, sampleRate * 0.45f, toneSm.getCurrentValue());
    coeff = std::exp (-2.f * juce::MathConstants<float>::pi * tone / sampleRate);
    if (varPtr != nullptr)
        bindings.prepare (varPtr, { &bitsExpr, &mixExpr, &toneExpr });
    clearRuntimeState();
}

float SignalChain::Bitcrush::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Bitcrush::processBlock (juce::AudioBuffer<float>& buffer)
{
    bindings.refresh();
    auto pull = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluateLive (0.f);
        return std::isfinite (v) ? v : fb;
    };
    bitsSm.setTargetValue (juce::jlimit (1.f, 16.f, pull (bitsExpr, 8.f)));
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, pull (mixExpr, 1.f)));
    toneSm.setTargetValue (juce::jlimit (800.f, sampleRate * 0.45f, pull (toneExpr, 8000.f)));
    const float tone = toneSm.getNextValue();
    coeff = std::exp (-2.f * juce::MathConstants<float>::pi * tone / sampleRate);

    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS = buffer.getNumSamples();
    for (int i = 0; i < nS; ++i)
    {
        const float bits = bitsSm.getNextValue();
        const float mix = mixSm.getNextValue();
        const float levels = std::pow (2.f, bits - 1.f);
        const float dryG = 1.f - mix;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            const float xn = juce::jlimit (-1.f, 1.f, std::isfinite (x) ? x : 0.f);
            const float q = std::round (xn * levels) / levels;
            float& z = ch == 0 ? zL : zR;
            z = coeff * z + (1.f - coeff) * q;
            buffer.setSample (ch, i, dryG * xn + mix * z);
        }
        if (nCh == 1)
            zR = zL;
    }
}
