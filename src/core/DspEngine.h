#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file DspEngine.h
    @brief Manages all real-time DSP processing for NeuroCore.

    Extracted from PluginProcessor to separate the DSP signal path
    from plugin infrastructure (APVTS, presets, MIDI learn, etc.).

    Owns:
    - Oversampling (juce::dsp::Oversampling)
    - ProcessorChain: InputGain → NoiseGate → SignalPolisher
    - InputRouter
    - DC-blocker and output low-pass filter
    - DryWetMixer
    - Output gain stages (auto-compensation + user gain)
    - Working buffers: dryBuffer, scriptBuffer, oldScriptBuffer
    - upChannelPtrs (pre-allocated, no heap in audio callback)
    - Smoothed parameters for a/b/c/d knobs
    - formulaBlend (for cross-fade between old and new formula)
    - bypassActive flag
    - Loudness / limiter / invalid-sample status (atomic)
*/

#include <JuceHeader.h>
#include "../dsp/InputGain.h"
#include "../dsp/InputRouter.h"
#include "../dsp/SignalPolisher.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include "../dsl/SignalChain.h"
#include <atomic>
#include <array>
#include <memory>

class DspEngine
{
public:
    DspEngine();

    /** Prepare for playback. Must be called on the message thread.
        @param spec          Target processing spec (before oversampling).
        @param vts           APVTS used to read parameters during processBlock.
        @param oversamplingStages  Number of oversampling stages (0 = off, 1 = 2×, 2 = 4×, 3 = 8×). */
    void prepare(const juce::dsp::ProcessSpec& spec,
                 juce::AudioProcessorValueTreeState& vts,
                 int oversamplingStages);

    /** Reset all processing state. */
    void reset(double sampleRate, int blockSize);

    /** Release all resources. */
    void release();

    /** Main audio processing call.
        Reads parameters from the APVTS stored during prepare().
        @param buffer         In/out audio buffer.
        @param signalChain    Current DSL signal chain.
        @param oldSignalChain Previous DSL signal chain (used during cross-fade). */
    void processBlock(juce::AudioBuffer<float>& buffer,
                      dsl::SignalChain& signalChain,
                      dsl::SignalChain& oldSignalChain);

    /** Called after a new formula is successfully loaded so the engine can
        start the cross-fade from oldSignalChain to the new signalChain.
        Also resets low-pass filter and oversampler state. */
    void onFormulaChanged();

    /** Returns current oversampling factor (1 = no oversampling). */
    size_t getOversamplingFactor() const noexcept;

    /** Returns the latency introduced by oversampling in samples. */
    int getOversamplingLatency() const noexcept;

    /** Returns the current processing spec (before oversampling). */
    juce::dsp::ProcessSpec getCurrentSpec() const noexcept { return currentSpec; }

    /** Returns the current oversampling index (0..3). */
    int getOversamplingIndex() const noexcept { return oversamplingIndex.load(); }
    void setOversamplingIndex(int idx) noexcept { oversamplingIndex.store(idx); }

    // Status queries (RT-safe)
    float getLoudnessDb()     const noexcept { return lastLoudness.load(); }
    bool  isLimiterActive()   const noexcept { return limiterActive.load(); }
    bool  consumeInvalidFlag() noexcept      { return invalidFlag.exchange(false); }

    /** Temporarily suppress the wet signal (used during validation). */
    void setValidationBypass(bool enable);

    // Accessors to individual DSP chain elements (used by PluginProcessor delegation)
    InputRouter&    getInputRouter()    noexcept { return inputRouter; }
    InputGain&      getInputGain()      noexcept { return chain.get<0>(); }
    juce::dsp::NoiseGate<float>& getNoiseGate() noexcept { return chain.get<1>(); }
    SignalPolisher& getPolisher()       noexcept { return chain.get<2>(); }

private:
    juce::dsp::ProcessSpec currentSpec { Config::kDefaultSampleRate,
                                         static_cast<juce::uint32>(Config::kDefaultBlockSize),
                                         static_cast<juce::uint32>(Config::kMaxChannels) };

    juce::AudioProcessorValueTreeState* apvts { nullptr };

    // DSP chain: InputGain → NoiseGate → SignalPolisher
    juce::dsp::ProcessorChain<InputGain, juce::dsp::NoiseGate<float>, SignalPolisher> chain;
    InputRouter inputRouter;

    // Filters
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> lowpassFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> dcBlocker;

    // Dry/Wet mixing
    juce::dsp::DryWetMixer<float> dryWetMixer;
    int dryWetLatency { 0 };
    juce::SmoothedValue<float> wetValue;

    // Gain stages
    juce::SmoothedValue<float> gainCompValue;
    juce::dsp::Gain<float>     outputGain;
    juce::dsp::Gain<float>     userOutputGain;
    juce::SmoothedValue<float> userGainValue;

    // Working buffers
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> scriptBuffer;
    juce::AudioBuffer<float> oldScriptBuffer;
    std::array<float*, Config::kMaxChannels> upChannelPtrs{};

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::atomic<int> oversamplingIndex { 1 }; // default 2× (index 1)

    // Formula cross-fade
    juce::SmoothedValue<float> formulaBlend;
    bool bypassActive { false };

    // Smoothed parameters for DSL a/b/c/d knobs
    std::array<juce::SmoothedValue<float>, 4> smoothedParams;

    // Status
    std::atomic<float> lastLoudness  { -100.0f };
    std::atomic<bool>  limiterActive { false };
    std::atomic<bool>  invalidFlag   { false };
};
