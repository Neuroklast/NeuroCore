#include <JuceHeader.h>
#include "BandPassFilter.h"
#include "../utils/Log.h"

void BandPassFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    if (sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("BandPassFilter::prepare received invalid ProcessSpec");
        return;
    }
    filter.prepare (spec);
    filter.reset();
    *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeBandPass(sampleRate, cutoff, q);
}

void BandPassFilter::reset()
{
    filter.reset();
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeBandPass(sampleRate, cutoff, q);
}

void BandPassFilter::process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept
{
    if (bypassed)
        return;
    filter.process (context);
}

void BandPassFilter::setCutoff(float freq) noexcept
{
    const float nyquist = sampleRate * 0.5f;
    cutoff = juce::jlimit(20.0f, nyquist, freq);
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeBandPass(sampleRate, cutoff, q);
}

void BandPassFilter::setQ(float newQ) noexcept
{
    q = juce::jlimit(0.1f, 10.0f, newQ);
    if (sampleRate > 0.0)
        *filter.state = *juce::dsp::IIR::Coefficients<SampleType>::makeBandPass(sampleRate, cutoff, q);
}
