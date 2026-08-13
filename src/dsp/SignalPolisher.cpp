#include <JuceHeader.h>
#include "SignalPolisher.h"
#include "../utils/Log.h"


void SignalPolisher::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    if (sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("SignalPolisher::prepare received invalid ProcessSpec");
        return;
    }

    limiter.prepare(spec);
    // Soft musical ceiling — avoids constant red-meter pump on amp sims
    limiter.setThreshold(-1.0f); // dB
    limiter.setRelease(80.0f);   // ms
    lastGood.assign(spec.numChannels, 0.0f);
    smoothRecovery.resize(spec.numChannels);
    for (auto& s : smoothRecovery)
    {
        s.reset(sampleRate, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
}

void SignalPolisher::reset()
{
    limiter.reset();
    std::fill(lastGood.begin(), lastGood.end(), 0.0f);
    for (auto& s : smoothRecovery)
    {
        auto rate = sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate;
        s.reset(rate, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
}

void SignalPolisher::process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept
{
    if (bypassed)
        return;

    auto& block = context.getOutputBlock();
    const size_t numSamples = block.getNumSamples();

    float peakBefore = 0.0f;
    if (mode == Limiter)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
                peakBefore = juce::jmax (peakBefore, std::abs (data[i]));
        }

        juce::dsp::ProcessContextReplacing<SampleType> ctx (block);
        limiter.process (ctx);
    }
    else if (mode == HardClip)
    {
        // atan soft ceiling ±1 (same family as engine softclip — low HF)
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                if (! std::isfinite (x))
                {
                    x = lastGood[ch];
                    invalidSample.store (true);
                }
                constexpr float k = 1.57079632679f;
                constexpr float s = 0.63661977237f;
                data[i] = s * std::atan (k * juce::jlimit (-40.0f, 40.0f, x));
            }
        }
    }

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* data = block.getChannelPointer (ch);
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto v = data[i];
            // Gold standard: never emit NaN/Inf — hold last good sample (no zero click)
            if (! std::isfinite (v))
            {
                invalidSample.store (true);
                v = lastGood[ch];
            }
            lastGood[ch] = v;
            data[i] = v;
        }
    }

    if (mode == Limiter)
    {
        float peakAfter = 0.0f;
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
                peakAfter = juce::jmax (peakAfter, std::abs (data[i]));
        }
        if (peakAfter < peakBefore - 1e-5f)
            limiterHit.store (true);
    }
}

void SignalPolisher::setParameter (const std::string& id, float v)
{
    if (id == EffectParameters::polisherMode)
        // limit mode to available options
        mode = juce::jlimit(0, 2, static_cast<int>(v));
}
