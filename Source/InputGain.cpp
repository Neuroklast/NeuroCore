#include "InputGain.h"
#include "Config.h"

void InputGain::prepare(const juce::dsp::ProcessSpec& spec)
{
    smoothedGain.reset(spec.sampleRate, Config::kSmoothingTime);
    smoothedGain.setCurrentAndTargetValue(1.0f);
}

void InputGain::reset() noexcept
{
    smoothedGain.reset();
    smoothedGain.setCurrentAndTargetValue(1.0f);
}

