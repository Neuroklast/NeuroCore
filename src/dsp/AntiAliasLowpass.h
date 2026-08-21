#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"
#include <array>

class AntiAliasLowpass
{
public:
    void prepare (double osSampleRate, juce::uint32 maxOsBlock,
                  juce::uint32 numChannels, double hostSampleRate, bool live) noexcept;
    void reset() noexcept;
    void process (juce::dsp::AudioBlock<float>& osBlock) noexcept;
    double cutoffHz() const noexcept { return cutoff; }

private:
    static constexpr int kMaxSos = 12;
    using Stage = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                 juce::dsp::IIR::Coefficients<float>>;
    std::array<Stage, kMaxSos> stages;
    int numStages { 0 };
    double cutoff { 0.0 };
};
