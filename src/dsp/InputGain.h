#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include "../core/EffectParameters.h"
#include "../core/Config.h"

/// Simple processor applying a smoothed gain to the incoming audio.
/// The gain value is updated via setParameter and interpolated to
/// avoid clicks when changed.
class InputGain : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;
    InputGain() = default;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept override;
    void processBlock (juce::AudioBuffer<SampleType>& buffer)
    {
        juce::dsp::AudioBlock<SampleType> block(buffer);
        juce::dsp::ProcessContextReplacing<SampleType> ctx(block);
        process(ctx);
    }

    void setParameter (const std::string& id, float v);
    void setBypassed (bool b) noexcept { bypassed = b; }

private:
    bool bypassed { false };

        float sampleRate = Config::kDefaultSampleRate;
    juce::SmoothedValue<SampleType> smoothedGain;
    SampleType targetGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputGain);
};
