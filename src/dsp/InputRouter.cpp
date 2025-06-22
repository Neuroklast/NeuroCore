#include "InputRouter.h"
#include "../utils/Log.h"

void InputRouter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize  = static_cast<int>(spec.maximumBlockSize);

    if (sampleRate <= 0.0 || blockSize <= 0 || spec.numChannels < 1)
        logError("InputRouter::prepare received invalid ProcessSpec");
}

void InputRouter::reset()
{
    if (sampleRate <= 0.0)
        sampleRate = Config::kDefaultSampleRate;
    if (blockSize <= 0)
        blockSize = Config::kDefaultBlockSize;

    channelEnabled = { true, true };
    bypassed = false;
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

