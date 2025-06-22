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
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
                data[i] = juce::jlimit (-1.0f, 1.0f, data[i]);
        }
    }

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* data = block.getChannelPointer (ch);
        auto& smoother = smoothRecovery[ch];
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto v = data[i];
            if (! std::isfinite (v))
            {
                invalidSample.store (true);
                smoother.setCurrentAndTargetValue(lastGood[ch]);
                smoother.setTargetValue(0.f);
                v = smoother.getNextValue();
            }
            else
            {
                lastGood[ch] = v;
                if (smoother.isSmoothing())
                    v = smoother.getNextValue();
            }
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
        mode = static_cast<int> (v);
}
