#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file DspEngine.h
    @brief Manages all real-time DSP processing for NeuroCore.

    Owns:
    - Oversampling (juce::dsp::Oversampling)
    - InputGain (host rate, before dry split)
    - SignalPolisher (post-DSL, oversampled)
    - InputRouter
    - DC-blocker and anti-alias LPF (oversampled)
    - LatencyAlignedSidechain dry (timeline == OS wet latency)
    - Continuous dry/wet mix
    - AutoGain (strength from APVTS, default off)
    - OutputSanitizer (sole peak safety when polisher is None)
    - Working buffers + switchRamp
*/

#include <JuceHeader.h>
#include "../dsp/InputGain.h"
#include "../dsp/InputRouter.h"
#include "../dsp/SignalPolisher.h"
#include "../dsp/OutputSanitizer.h"
#include "../dsp/LatencyAlignedSidechain.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include "../dsl/SignalChain.h"
#include "../utils/AudioDiagnostics.h"
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
        @param signalChain    Current DSL signal chain. */
    void processBlock(juce::AudioBuffer<float>& buffer,
                      dsl::SignalChain& signalChain);

    /** Called after a new formula is successfully loaded.
        Soft-engages the new chain (switchRamp); does not dual-run an old chain. */
    void onFormulaChanged();

    /** Returns current oversampling factor (1 = no oversampling). */
    size_t getOversamplingFactor() const noexcept;

    /** Returns the latency introduced by oversampling in samples. */
    int getOversamplingLatency() const noexcept { return osLatencySamples; }

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

    /** NaN / jump / crackle logger (RT-safe ring + file flush). */
    AudioDiagnostics& getDiagnostics() noexcept { return diagnostics; }
    const AudioDiagnostics& getDiagnostics() const noexcept { return diagnostics; }

    InputRouter&    getInputRouter()    noexcept { return inputRouter; }
    InputGain&      getInputGain()      noexcept { return chain.get<0>(); }
    SignalPolisher& getPolisher()       noexcept { return chain.get<1>(); }

private:
    juce::dsp::ProcessSpec currentSpec { Config::kDefaultSampleRate,
                                         static_cast<juce::uint32>(Config::kDefaultBlockSize),
                                         static_cast<juce::uint32>(Config::kMaxChannels) };

    juce::AudioProcessorValueTreeState* apvts { nullptr };

    // ProcessorChain: [0] InputGain, [1] SignalPolisher
    juce::dsp::ProcessorChain<InputGain, SignalPolisher> chain;
    InputRouter inputRouter;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> lowpassFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> dcBlocker;

    juce::SmoothedValue<float> wetValue;

    juce::SmoothedValue<float> gainCompValue;
    juce::dsp::Gain<float>     outputGain;
    juce::dsp::Gain<float>     userOutputGain;
    juce::SmoothedValue<float> userGainValue;

    OutputSanitizer outputSanitizer;

    std::array<float, Config::kMaxChannels> postDslLastGood {};

    juce::AudioBuffer<float> dryBuffer;
    LatencyAlignedSidechain drySidechain;
    juce::AudioBuffer<float> scriptBuffer;

    /** [1]=2× [2]=4× [3]=8× — built once per host spec so OS switches do not realloc. */
    std::unique_ptr<juce::dsp::Oversampling<float>> osBank[4];
    juce::dsp::Oversampling<float>* oversampler { nullptr };
    std::atomic<int> oversamplingIndex { Config::kDefaultOversamplingIndex };
    /** Integer OS latency used by the dry sidechain and setLatencySamples. */
    int osLatencySamples { 0 };
    juce::uint32 lastOsBankBlock { 0 };
    juce::uint32 lastOsBankCh { 0 };

    /** 0→1 ramp after formula/OS change — kills loudness spike & zipper. */
    juce::SmoothedValue<float> switchRamp;
    bool bypassActive { false };

    std::array<juce::SmoothedValue<float>, Config::kNumUserParams> smoothedParams;

    std::atomic<float> lastLoudness  { -100.0f };
    std::atomic<bool>  limiterActive { false };
    std::atomic<bool>  invalidFlag   { false };

    int limiterHoldBlocks { 0 };

    AudioDiagnostics diagnostics;
    std::array<float, Config::kMaxChannels> diagLastIn  {};
    std::array<float, Config::kMaxChannels> diagLastPost {};
    std::array<float, Config::kMaxChannels> diagLastOut {};

    void publishLoudness (float instantDb, int numSamples) noexcept;
};
