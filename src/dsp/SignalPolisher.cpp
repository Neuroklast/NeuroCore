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

    if (mode == Limiter)
    {
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
                v = lastGood[ch];
            else
                lastGood[ch] = v;
            data[i] = v;
        }
    }
}

void SignalPolisher::setParameter (const std::string& id, float v)
{
    if (id == EffectParameters::polisherMode)
        mode = static_cast<int> (v);
}
