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
    void processBlock (juce::AudioBuffer<SampleType>& buffer)
    {
        juce::dsp::AudioBlock<SampleType> block(buffer);
        juce::dsp::ProcessContextReplacing<SampleType> ctx(block);
        process(ctx);
    }

    void setUseLeft(bool enable) noexcept { channelEnabled[0] = enable; updateWeights(); }
    void setUseRight(bool enable) noexcept { channelEnabled[1] = enable; updateWeights(); }

private:
    std::array<bool,2> channelEnabled { true, true };
    double sampleRate { Config::kDefaultSampleRate };
    int    blockSize  { Config::kDefaultBlockSize };
    bool bypassed { false };
    std::array<std::array<juce::SmoothedValue<SampleType>,2>,2> weights; // [out][in]

    void updateWeights() noexcept;
};

