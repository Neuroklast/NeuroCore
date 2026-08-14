#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Ir::clearRuntimeState() noexcept
{
    conv.reset();
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

    juce::AudioBuffer<float> copy (ir.getNumChannels(), ir.getNumSamples());
    for (int c = 0; c < ir.getNumChannels(); ++c)
        copy.copyFrom (c, 0, ir, c, 0, ir.getNumSamples());

    conv.loadImpulseResponse (std::move (copy), irSr,
                              juce::dsp::Convolution::Stereo::yes,
                              juce::dsp::Convolution::Trim::no,
                              juce::dsp::Convolution::Normalise::no);
    hasIr = true;
    latencySamples = (int) conv.getLatency();
}

void SignalChain::Ir::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    conv.prepare (spec);
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
        for (int i = 0; i < nS; ++i)
        {
            mixSm.getNextValue();
            gainSm.getNextValue();
        }
        return;
    }

    juce::AudioBuffer<float> dry (nCh, nS);
    for (int c = 0; c < nCh; ++c)
        dry.copyFrom (c, 0, buffer, c, 0, nS);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    conv.process (ctx);

    for (int i = 0; i < nS; ++i)
    {
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
        const float g = juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 24.f, gainSm.getNextValue()));
        for (int c = 0; c < nCh; ++c)
        {
            const float w = buffer.getSample (c, i) * g;
            const float d = dry.getSample (c, i);
            float y = d * (1.f - mix) + w * mix;
            if (! std::isfinite (y))
                y = d;
            buffer.setSample (c, i, y);
        }
    }
}
