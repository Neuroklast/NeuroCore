#include "SignalPolisher.h"
#include "../utils/Log.h"

void SignalPolisher::prepare (const juce::dsp::ProcessSpec& spec)
{
    if (spec.sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("SignalPolisher::prepare received invalid ProcessSpec");
        return;
    }

    limiter.prepare(spec);
    lastGood.assign(spec.numChannels, 0.0f);
}

void SignalPolisher::reset()
{
    limiter.reset();
    std::fill(lastGood.begin(), lastGood.end(), 0.0f);
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
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto v = data[i];
            if (! std::isfinite (v))
            {
                invalidSample.store (true);
                v = lastGood[ch];
            }
            else
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
        mode = static_cast<int> (v);
}
