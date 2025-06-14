#pragma once
#include <JuceHeader.h>

class SignalPolisher
{
public:
    enum Mode { None = 0, HardClip, Limiter };
    using SampleType = float;

    SignalPolisher() = default;

    void setModePointer(std::atomic<float>* ptr) noexcept { modeParam = ptr; }

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        auto& block = context.getOutputBlock();
        const size_t numSamples = block.getNumSamples();

        auto mode = modeParam ? static_cast<int>(modeParam->load()) : 0;
        if (mode == Limiter)
        {
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            limiter.process(ctx);
        }
        else if (mode == HardClip)
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* data = block.getChannelPointer(ch);
                for (size_t i = 0; i < numSamples; ++i)
                    data[i] = juce::jlimit(-1.0f, 1.0f, data[i]);
            }
        }

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                auto v = data[i];
                if (!std::isfinite(v))
                    v = lastGood[ch];
                else
                    lastGood[ch] = v;
                data[i] = v;
            }
        }
    }

private:
    std::atomic<float>* modeParam { nullptr };
    juce::dsp::Limiter<SampleType> limiter;
    std::vector<SampleType> lastGood;
};

