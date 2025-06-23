#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer und Simon Seifried
*/
#include <JuceHeader.h>
#include "../core/Config.h"

class HighPassFilter : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;
    HighPassFilter() = default;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept override;

    void setCutoff(float freq) noexcept;
    void setBypassed(bool b) noexcept { bypassed = b; }

private:
    float sampleRate = Config::kDefaultSampleRate;
    float cutoff     = 20.0f;
    bool  bypassed   = false;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<SampleType>,
                                   juce::dsp::IIR::Coefficients<SampleType>> filter;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HighPassFilter)
};
