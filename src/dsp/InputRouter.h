#pragma once
#include <JuceHeader.h>
#include "../core/Config.h"

class InputRouter : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;
    InputRouter() = default;

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::ProcessContextReplacing<SampleType>& ctx) noexcept override;

    void setUseLeft(bool enable) noexcept { channelEnabled[0] = enable; }
    void setUseRight(bool enable) noexcept { channelEnabled[1] = enable; }

private:
    std::array<bool,2> channelEnabled { true, true };
    double sampleRate { Config::kDefaultSampleRate };
    int    blockSize  { Config::kDefaultBlockSize };
    bool bypassed { false };
};

