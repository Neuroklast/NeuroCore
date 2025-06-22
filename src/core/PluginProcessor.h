/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <cmath>
#include <array>
#include <memory>
#include "../dsl/SignalChain.h"
#include "../utils/PresetManager.h"
#include "../dsp/InputGain.h"
#include "../dsp/InputRouter.h"
#include "../dsp/WaveShaper.h"
#include "../dsp/SignalPolisher.h"
#include "EffectParameters.h"
#include "Config.h"


//==============================================================================
/**
*/
class NeuroCoreAudioProcessor  : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AsyncUpdater
{
public:
    //==============================================================================
    NeuroCoreAudioProcessor();
    ~NeuroCoreAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Updates signal chain script from the UI
    bool setFormula (const juce::String& text, juce::String& error);
    juce::String getScript() const noexcept { return dslScript; }

    void setVariableName(int index, const juce::String& name);
    juce::String getVariableName(int index) const noexcept
    {
        if (juce::isPositiveAndBelow (index, (int) variableNames.size()))
            return variableNames[(size_t) index];
        return {};
    }
    const std::array<juce::String, 4>& getVariableNames() const noexcept { return variableNames; }

    void loadLanguage(const juce::String& lang);
    juce::String getCurrentLanguage() const noexcept { return currentLanguage; }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Evaluates current formula for a single sample value.
    float evaluateFormula (float x);

    void getInputWaveform(juce::AudioBuffer<float>& dest);
    void getOutputWaveform(juce::AudioBuffer<float>& dest);

    float getLoudnessDb()   const noexcept { return lastLoudness.load(); }
    bool  isLimiterActive() const noexcept { return limiterActive.load(); }
    bool  consumeInvalidFlag() noexcept { return invalidFlag.exchange(false); }

    // juce::AudioProcessorValueTreeState::Listener implementation
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // Returns the last process specification
    juce::dsp::ProcessSpec getCurrentSpec() const noexcept { return currentSpec; }

private:
    //==============================================================================
    std::array<std::atomic<float>, 4> parameterValues{};
    std::array<juce::String, 4> variableNames{ Config::kDefaultVariableNames[0],
                                               Config::kDefaultVariableNames[1],
                                               Config::kDefaultVariableNames[2],
                                               Config::kDefaultVariableNames[3] };
    std::array<juce::SmoothedValue<float>, 4> smoothedParams;
    juce::String dslScript;
    juce::String currentLanguage;


    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::atomic<int> oversamplingIndex { (int) std::log2 (Config::kOversamplingFactor) };
    juce::dsp::DryWetMixer<float> dryWetMixer;
    int                          dryWetLatency { 0 };
    juce::SmoothedValue<float>    wetValue;
    juce::SmoothedValue<float>    gainCompValue;
    juce::dsp::Gain<float>        outputGain;
    juce::dsp::Gain<float>        userOutputGain;
    juce::AudioBuffer<float>      dryBuffer;
    juce::AudioBuffer<float>      inputWaveBuffer;
    juce::AudioBuffer<float>      outputWaveBuffer;
    juce::AudioBuffer<float>      scriptBuffer; // buffer for DSL processing
    std::atomic<int>             inputWrite { 0 };
    std::atomic<int>             outputWrite { 0 };
    InputRouter                   inputRouter;
    juce::dsp::ProcessorChain<InputGain, SignalPolisher> chain;
    dsl::SignalChain              signalChain;

    std::atomic<float> lastLoudness { -100.0f };
    std::atomic<bool>  limiterActive { false };
    std::atomic<bool>  invalidFlag   { false };


    // Helper accessors for the processor chain
    InputRouter&    getInputRouter()    noexcept { return inputRouter; }
    InputGain&      getInputGain()      noexcept { return chain.get<0>(); }
    SignalPolisher& getPolisher()       noexcept { return chain.get<1>(); }


    juce::dsp::ProcessSpec currentSpec { Config::kDefaultSampleRate,
                                         static_cast<juce::uint32> (Config::kDefaultBlockSize),
                                         static_cast<juce::uint32> (Config::kMaxChannels) };

    void updateProcessingSpec (double sampleRate, int blockSize);
    void handleAsyncUpdate() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessor)
};
