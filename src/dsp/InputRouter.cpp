#include <JuceHeader.h>
#include "InputRouter.h"
#include "../utils/Log.h"
#include <cmath>

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

    // BOTH + mono guitar: one side is digital zero. Seed it so L/R splits work.
    if (channelEnabled[0] && channelEnabled[1] && numSamples > 0)
    {
        float lE = 0.f, rE = 0.f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            lE = juce::jmax (lE, std::abs (left[i]));
            rE = juce::jmax (rE, std::abs (right[i]));
        }
        if (rE < 1.0e-5f && lE > 1.0e-5f)
            juce::FloatVectorOperations::copy (right, left, (int) numSamples);
        else if (lE < 1.0e-5f && rE > 1.0e-5f)
            juce::FloatVectorOperations::copy (left, right, (int) numSamples);
    }

    for (size_t i = 0; i < numSamples; ++i)
    {
        auto wl0 = weights[0][0].getNextValue();
        auto wr0 = weights[0][1].getNextValue();
        auto wl1 = weights[1][0].getNextValue();
        auto wr1 = weights[1][1].getNextValue();

        auto l = left[i];
        auto r = right[i];

        left[i]  = l * wl0 + r * wr0;
        right[i] = l * wl1 + r * wr1;
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

