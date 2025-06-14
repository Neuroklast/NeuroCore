#include "SignalPolisher.h"

void SignalPolisher::prepare(const juce::dsp::ProcessSpec& spec)
{
    limiter.prepare(spec);
    lastGood.assign(spec.numChannels, 0.0f);
}

void SignalPolisher::reset()
{
    limiter.reset();
    std::fill(lastGood.begin(), lastGood.end(), 0.0f);
}

