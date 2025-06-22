#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include "../core/EffectParameters.h"

class SignalPolisher : public juce::dsp::ProcessorBase
{
public:
    enum Mode { None = 0, HardClip, Limiter };
    using SampleType = float;
    SignalPolisher() = default;

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept override;

    void setParameter (const std::string& id, float v);
    void setBypassed (bool b) noexcept { bypassed = b; }

    bool wasLimiterHit() noexcept { return limiterHit.exchange(false); }
    bool wasInvalidSample() noexcept { return invalidSample.exchange(false); }

private:
    int mode { None };
    bool bypassed { false };
    juce::dsp::Limiter<SampleType> limiter;
    std::vector<SampleType> lastGood;
    std::vector<juce::SmoothedValue<SampleType>> smoothRecovery;
    std::atomic<bool> limiterHit { false };
    std::atomic<bool> invalidSample { false };
    double sampleRate { Config::kDefaultSampleRate };
};
