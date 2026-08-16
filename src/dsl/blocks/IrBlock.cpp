#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Ir::clearRuntimeState() noexcept
{
    conv.reset();
    dryAlign.reset();
}

void SignalChain::Ir::clearImpulse()
{
    hasIr = false;
    latencySamples = 0;
    conv.reset();
}

void SignalChain::Ir::loadImpulse (const juce::AudioBuffer<float>& ir, double irSr)
{
    if (ir.getNumSamples() <= 0 || ir.getNumChannels() <= 0 || irSr <= 0.0)
    {
        clearImpulse();
        return;
    }

    const int n = ir.getNumSamples();
    juce::AudioBuffer<float> copy (2, n);
    copy.copyFrom (0, 0, ir, 0, 0, n);
    if (ir.getNumChannels() > 1)
        copy.copyFrom (1, 0, ir, 1, 0, n);
    else
        copy.copyFrom (1, 0, ir, 0, 0, n);

    conv.loadImpulseResponse (std::move (copy), irSr,
                              juce::dsp::Convolution::Stereo::yes,
                              juce::dsp::Convolution::Trim::no,
                              juce::dsp::Convolution::Normalise::no);
    hasIr = true;
    latencySamples = (int) conv.getLatency();
    if (dryScratch.getNumChannels() > 0 && dryScratch.getNumSamples() > 0)
        dryAlign.prepare (dryScratch.getNumChannels(), dryScratch.getNumSamples(),
                          juce::jmax (0, latencySamples));
}

void SignalChain::Ir::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    conv.prepare (spec);
    const int ch = (int) juce::jmax ((juce::uint32) 1, spec.numChannels);
    const int n  = (int) juce::jmax ((juce::uint32) 1, spec.maximumBlockSize);
    dryScratch.setSize (ch, n, false, false, true);
    dryAlign.prepare (ch, n, juce::jmax (0, latencySamples));
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    gainSm.reset (sampleRate, Config::kSmoothingTime);
    const float m = mixExpr.evaluate (0.f);
    const float g = gainDb.evaluate (0.f);
    mixSm.setCurrentAndTargetValue (std::isfinite (m) ? m : 1.f);
    gainSm.setCurrentAndTargetValue (std::isfinite (g) ? g : 0.f);
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
}

float SignalChain::Ir::process (int, float x) { return x; }

void SignalChain::Ir::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    if (varPtr != nullptr)
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            mixExpr.setVariable (n.second, v);
            gainDb.setVariable (n.second, v);
        }

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fb;
    };
    mixSm.setTargetValue (ev (mixExpr, 1.f));
    gainSm.setTargetValue (ev (gainDb, 0.f));

    if (! hasIr)
    {
        mixSm.skip (nS);
        gainSm.skip (nS);
        return;
    }

    if (dryScratch.getNumChannels() < nCh || dryScratch.getNumSamples() < nS)
        dryScratch.setSize (nCh, nS, false, false, true);
    for (int c = 0; c < nCh; ++c)
        dryScratch.copyFrom (c, 0, buffer, c, 0, nS);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    conv.process (ctx);

    const juce::AudioBuffer<float>* drySrc = &dryScratch;
    if (latencySamples > 0)
    {
        dryAlign.pushAndRead (dryScratch, nS);
        drySrc = &dryAlign.getAligned();
    }

    const bool staticMix = ! mixSm.isSmoothing() && ! gainSm.isSmoothing();
    const float mix0 = juce::jlimit (0.f, 1.f, mixSm.getCurrentValue());
    const float g0 = juce::Decibels::decibelsToGain (
        juce::jlimit (-24.f, 24.f, gainSm.getCurrentValue()));
    if (staticMix && mix0 <= 1.0e-4f)
    {
        for (int c = 0; c < nCh; ++c)
            buffer.copyFrom (c, 0, *drySrc, c, 0, nS);
        mixSm.skip (nS);
        gainSm.skip (nS);
        return;
    }
    if (staticMix && mix0 >= 0.999f)
    {
        if (std::abs (g0 - 1.f) > 1.0e-5f)
            buffer.applyGain (g0);
        mixSm.skip (nS);
        gainSm.skip (nS);
        return;
    }

    float* chOut[2] {};
    const float* chDry[2] {};
    const int useCh = juce::jmin (nCh, 2);
    for (int c = 0; c < useCh; ++c)
    {
        chOut[c] = buffer.getWritePointer (c);
        chDry[c] = drySrc->getReadPointer (c);
    }

    for (int i = 0; i < nS; ++i)
    {
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
        const float g = juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 24.f, gainSm.getNextValue()));
        const float dryG = 1.f - mix;
        for (int c = 0; c < useCh; ++c)
        {
            const float d = chDry[c][i];
            float y = d * dryG + chOut[c][i] * g * mix;
            if (! std::isfinite (y))
                y = d;
            chOut[c][i] = y;
        }
    }
}
