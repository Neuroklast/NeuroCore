#include "InputRouter.h"

void InputRouter::prepare(const juce::dsp::ProcessSpec& spec)
{
    juce::ignoreUnused(spec);
}

void InputRouter::reset()
{
}

void InputRouter::process(const juce::dsp::ProcessContextReplacing<SampleType>& ctx) noexcept
{
    auto& block = ctx.getOutputBlock();
    if (block.getNumChannels() < 2)
        return;

    const size_t numSamples = block.getNumSamples();
    auto* left  = block.getChannelPointer(0);
    auto* right = block.getChannelPointer(1);

    const bool useLeft  = channelEnabled[0];
    const bool useRight = channelEnabled[1];

    if (useLeft && useRight)
        return; // stereo

    if (useLeft && !useRight)
    {
        for (size_t i = 0; i < numSamples; ++i)
            right[i] = left[i];
        return;
    }

    if (!useLeft && useRight)
    {
        for (size_t i = 0; i < numSamples; ++i)
            left[i] = right[i];
        return;
    }

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        juce::FloatVectorOperations::clear(block.getChannelPointer(ch), (int)numSamples);
}

