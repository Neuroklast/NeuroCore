#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Xover::clearRuntimeState() noexcept
{
    for (auto& c : ch)
    {
        c.lp1a.reset(); c.lp1b.reset(); c.hp1a.reset(); c.hp1b.reset();
        c.lp2a.reset(); c.lp2b.reset(); c.hp2a.reset(); c.hp2b.reset();
    }
    lastF1 = -1.f;
    lastF2 = -1.f;
}

void SignalChain::Xover::applyCoeffs (float f1, float f2) noexcept
{
    const float ny = sampleRate * 0.45f;
    f1 = juce::jlimit (20.f, ny, f1);
    f2 = juce::jlimit (f1 + 10.f, ny, f2);
    constexpr float q = 0.70710678f;
    auto lp1 = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, f1, q);
    auto hp1 = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f1, q);
    auto lp2 = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, f2, q);
    auto hp2 = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f2, q);
    for (auto& c : ch)
    {
        *c.lp1a.coefficients = *lp1;
        *c.lp1b.coefficients = *lp1;
        *c.hp1a.coefficients = *hp1;
        *c.hp1b.coefficients = *hp1;
        *c.lp2a.coefficients = *lp2;
        *c.lp2b.coefficients = *lp2;
        *c.hp2a.coefficients = *hp2;
        *c.hp2b.coefficients = *hp2;
    }
    lastF1 = f1;
    lastF2 = f2;
}

void SignalChain::Xover::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    const juce::dsp::ProcessSpec one { spec.sampleRate, spec.maximumBlockSize, 1 };
    for (auto& c : ch)
    {
        c.lp1a.prepare (one); c.lp1b.prepare (one);
        c.hp1a.prepare (one); c.hp1b.prepare (one);
        c.lp2a.prepare (one); c.lp2b.prepare (one);
        c.hp2a.prepare (one); c.hp2b.prepare (one);
    }
    f1Sm.reset (sampleRate, Config::kSmoothingTime);
    f2Sm.reset (sampleRate, Config::kSmoothingTime);
    const float a = f1Hz.evaluate (0.f);
    const float b = f2Hz.evaluate (0.f);
    f1Sm.setCurrentAndTargetValue (std::isfinite (a) && a > 0.f ? a : 120.f);
    f2Sm.setCurrentAndTargetValue (std::isfinite (b) && b > 0.f ? b : 2500.f);
    applyCoeffs (f1Sm.getCurrentValue(), f2Sm.getCurrentValue());
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
}

float SignalChain::Xover::process (int, float x) { return x; }

void SignalChain::Xover::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    if (varPtr != nullptr)
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            f1Hz.setVariable (n.second, v);
            f2Hz.setVariable (n.second, v);
        }

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fb;
    };
    f1Sm.setTargetValue (ev (f1Hz, 120.f));
    f2Sm.setTargetValue (ev (f2Hz, 2500.f));
    const float f1 = f1Sm.getNextValue();
    const float f2 = f2Sm.getNextValue();
    if (nS > 1)
    {
        f1Sm.skip (nS - 1);
        f2Sm.skip (nS - 1);
    }
    if (std::abs (f1 - lastF1) > 0.8f || std::abs (f2 - lastF2) > 0.8f)
        applyCoeffs (f1, f2);

    const int useCh = juce::jmin (nCh, 2);
    for (int c = 0; c < useCh; ++c)
    {
        auto& p = ch[c];
        auto* in = buffer.getReadPointer (c);
        float* lo = (lowOut != nullptr && lowOut->getNumChannels() > c)
                        ? lowOut->getWritePointer (c) : nullptr;
        float* md = (threeBand && midOut != nullptr && midOut->getNumChannels() > c)
                        ? midOut->getWritePointer (c) : nullptr;
        float* hi = (highOut != nullptr && highOut->getNumChannels() > c)
                        ? highOut->getWritePointer (c) : nullptr;
        const int nLo = lo != nullptr ? juce::jmin (nS, lowOut->getNumSamples()) : 0;
        const int nMd = md != nullptr ? juce::jmin (nS, midOut->getNumSamples()) : 0;
        const int nHi = hi != nullptr ? juce::jmin (nS, highOut->getNumSamples()) : 0;

        for (int i = 0; i < nS; ++i)
        {
            const float x = in[i];
            const float low = p.lp1b.processSample (p.lp1a.processSample (x));
            const float hp1 = p.hp1b.processSample (p.hp1a.processSample (x));
            if (i < nLo) lo[i] = low;
            if (threeBand)
            {
                const float mid = p.lp2b.processSample (p.lp2a.processSample (hp1));
                const float high = p.hp2b.processSample (p.hp2a.processSample (x));
                if (i < nMd) md[i] = mid;
                if (i < nHi) hi[i] = high;
            }
            else if (i < nHi)
            {
                hi[i] = hp1;
            }
        }
    }
}
