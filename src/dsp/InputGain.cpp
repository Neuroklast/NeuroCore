#include <JuceHeader.h>
#include "InputGain.h"
#include "../utils/Log.h"

void InputGain::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    if (sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("InputGain::prepare received invalid ProcessSpec");
        return;
    }

    smoothedGain.reset(sampleRate, Config::kSmoothingTime);
    smoothedGain.setCurrentAndTargetValue(targetGain);
}

void InputGain::reset()
{
    smoothedGain.reset(sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate, Config::kSmoothingTime);
    smoothedGain.setCurrentAndTargetValue(targetGain);
}

void InputGain::process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept
{
    if (bypassed)
        return;

    auto& block = context.getOutputBlock();
    const size_t numSamples  = block.getNumSamples();
    const size_t numChannels = block.getNumChannels();

    std::vector<SampleType> gains(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
        gains[i] = smoothedGain.getNextValue();

    for (size_t ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::multiply(block.getChannelPointer(ch),
                                              gains.data(),
                                              static_cast<int>(numSamples));
}

void InputGain::setParameter (const std::string& id, float v)
{
    if (id == EffectParameters::inputGain)
    {
        // ensure gain stays within valid range
        targetGain = juce::jlimit(0.0f, 2.0f, v);
        smoothedGain.setTargetValue (targetGain);
    }
}
