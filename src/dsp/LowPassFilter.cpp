#include "LowPassFilter.h"
#include "../utils/Log.h"

void LowPassFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    if (sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("LowPassFilter::prepare received invalid ProcessSpec");
        return;
    }
    filter.prepare (spec);
    filter.reset();
    *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeLowPass(sampleRate, cutoff);
}

void LowPassFilter::reset()
{
    filter.reset();
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeLowPass(sampleRate, cutoff);
}

void LowPassFilter::process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept
{
    if (bypassed)
        return;
    filter.process (context);
}

void LowPassFilter::setCutoff(float freq) noexcept
{
    cutoff = freq;
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeLowPass(sampleRate, cutoff);
}
