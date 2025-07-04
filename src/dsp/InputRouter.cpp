#include <JuceHeader.h>
#include "InputRouter.h"
#include "../utils/Log.h"

void InputRouter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize  = static_cast<int>(spec.maximumBlockSize);

    if (sampleRate <= 0.0 || blockSize <= 0 || spec.numChannels < 1)
        logError("InputRouter::prepare received invalid ProcessSpec");

    for (auto& row : weights)
        for (auto& w : row)
        {
            w.reset(sampleRate, Config::kSmoothingTime);
            w.setCurrentAndTargetValue(0.f);
        }
    updateWeights();
}

void InputRouter::reset()
{
    if (sampleRate <= 0.0)
        sampleRate = Config::kDefaultSampleRate;
    if (blockSize <= 0)
        blockSize = Config::kDefaultBlockSize;

    channelEnabled = { true, true };
    bypassed = false;

    for (auto& row : weights)
        for (auto& w : row)
        {
            w.reset(sampleRate, Config::kSmoothingTime);
            w.setCurrentAndTargetValue(0.f);
        }
    updateWeights();
}

void InputRouter::process(const juce::dsp::ProcessContextReplacing<SampleType>& ctx) noexcept
{
    auto& block = ctx.getOutputBlock();
    if (block.getNumChannels() < 2)
        return;

    const size_t numSamples = block.getNumSamples();
    auto* left  = block.getChannelPointer(0);
    auto* right = block.getChannelPointer(1);

    std::vector<SampleType> wl0(numSamples), wr0(numSamples),
        wl1(numSamples), wr1(numSamples);

    for (size_t i = 0; i < numSamples; ++i)
    {
        wl0[i] = weights[0][0].getNextValue();
        wr0[i] = weights[0][1].getNextValue();
        wl1[i] = weights[1][0].getNextValue();
        wr1[i] = weights[1][1].getNextValue();
    }

    for (size_t i = 0; i < numSamples; ++i)
    {
        auto l = left[i];
        auto r = right[i];
        left[i]  = l * wl0[i] + r * wr0[i];
        right[i] = l * wl1[i] + r * wr1[i];
    }
}

void InputRouter::updateWeights() noexcept
{
    const bool useLeft  = channelEnabled[0];
    const bool useRight = channelEnabled[1];

    float ll = 0.f, lr = 0.f, rl = 0.f, rr = 0.f;

    if (useLeft && useRight) { ll = rr = 1.f; }
    else if (useLeft && !useRight) { ll = rl = 1.f; }
    else if (!useLeft && useRight) { lr = rr = 1.f; }
    else { /* silence */ }

    weights[0][0].setTargetValue(ll);
    weights[0][1].setTargetValue(lr);
    weights[1][0].setTargetValue(rl);
    weights[1][1].setTargetValue(rr);
}

