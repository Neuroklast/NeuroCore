#include <JuceHeader.h>
#include "HighPassFilter.h"
#include "../utils/Log.h"

void HighPassFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    if (sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("HighPassFilter::prepare received invalid ProcessSpec");
        return;
    }
    filter.prepare (spec);
    filter.reset();
    *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeHighPass(sampleRate, cutoff);
}

void HighPassFilter::reset()
{
    filter.reset();
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeHighPass(sampleRate, cutoff);
}

void HighPassFilter::process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept
{
    if (bypassed)
        return;
    filter.process (context);
}

void HighPassFilter::setCutoff(float freq) noexcept
{
    const float nyquist = sampleRate * 0.5f;
    cutoff = juce::jlimit(20.0f, nyquist, freq);
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeHighPass(sampleRate, cutoff);
}
