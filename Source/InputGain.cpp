#include "InputGain.h"

void InputGain::prepare(const juce::dsp::ProcessSpec& spec)
{
    smoothedGain.reset(spec.sampleRate, 0.02);
    smoothedGain.setCurrentAndTargetValue(1.0f);
}

void InputGain::reset() noexcept
{
    smoothedGain.reset();
    smoothedGain.setCurrentAndTargetValue(1.0f);
}

