#pragma once
#include <JuceHeader.h>
#include "../core/EffectParameters.h"

class InputGain : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;
    InputGain() = default;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept override;

    void setParameter (const std::string& id, float v);
    void setBypassed (bool b) noexcept { bypassed = b; }

private:
    bool bypassed { false };
    juce::SmoothedValue<SampleType> smoothedGain;
    SampleType targetGain { 1.0f };
};
