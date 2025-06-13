#pragma once
#include <JuceHeader.h>

// Simple input gain processor with smoothing
class InputGain
{
public:
    using SampleType = float;

    InputGain() = default;

    void setGainPointer(std::atomic<float>* ptr) noexcept { gainParam = ptr; }

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        auto& outBlock = context.getOutputBlock();
        const size_t numSamples = outBlock.getNumSamples();

        if (gainParam == nullptr)
            return;

        smoothedGain.setTargetValue(gainParam->load());

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            auto g = smoothedGain.getNextValue();
            for (size_t ch = 0; ch < outBlock.getNumChannels(); ++ch)
                outBlock.getChannelPointer(ch)[sample] *= g;
        }
    }

private:
    std::atomic<float>* gainParam { nullptr };
    juce::SmoothedValue<SampleType> smoothedGain;
};

