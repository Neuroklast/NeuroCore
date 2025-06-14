#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include "../core/Config.h"

// Wrapper around juce::dsp::Oscillator<float> allowing custom waveforms.
// Keeps oscillator and formula engine separated.
class AdvancedOscillatorWrapper : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;

    AdvancedOscillatorWrapper();

    // Prepare / reset / process
    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& ctx) noexcept override;

    // Forwarding methods
    void initialise (const std::function<SampleType (SampleType)>& func, size_t tableSize = 0);
    bool isInitialised() const noexcept;
    void setFrequency (SampleType newFreq, bool force = false) noexcept;
    SampleType getFrequency() const noexcept;

    // LFO parameters
    void setAmplitude (SampleType newAmp) noexcept;
    SampleType getAmplitude() const noexcept { return ampTarget; }
    void setPhase (SampleType radians) noexcept;
    SampleType getPhase() const noexcept { return phase.getCurrentPhase(); }
    void sync() noexcept; // reset phase

    // Custom waveform
    void setCustomFunction (std::function<SampleType (SampleType phase, SampleType input)> func) noexcept;
    void clearCustomFunction() noexcept;

private:
    juce::dsp::Oscillator<SampleType> osc;            // internal JUCE oscillator
    std::function<SampleType (SampleType, SampleType)> customFn;
    std::atomic<bool> useCustom { false };            // thread-safe flag

    juce::SmoothedValue<SampleType> freq;
    juce::SmoothedValue<SampleType> amp;
    SampleType freqTarget { 440.0f };
    SampleType ampTarget  { 1.0f };

    juce::dsp::Phase<SampleType> phase;
    SampleType sampleRate { Config::kDefaultSampleRate };
};

