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
#include "../utils/ExpressionEvaluator.h"
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
class NeuroCoreAudioProcessor  : public juce::AudioProcessor
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

    // Updates expression from the UI
    void setFormula (const juce::String& text);

    void setVariableName(int index, const juce::String& name);
    juce::String getVariableName(int index) const noexcept { return variableNames[index]; }
    const std::array<juce::String, 4>& getVariableNames() const noexcept { return variableNames; }
    const ExpressionEvaluator& getEvaluator() const noexcept { return *evaluator; }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Evaluates current formula for a single sample value.
    float evaluateFormula (float x);

    // Returns the last process specification
    juce::dsp::ProcessSpec getCurrentSpec() const noexcept { return currentSpec; }

private:
    //==============================================================================
    std::shared_ptr<ExpressionEvaluator> evaluator;
    std::array<std::atomic<float>, 4> parameterValues{};
    std::array<juce::String, 4> variableNames{ Config::kDefaultVariableNames[0],
                                               Config::kDefaultVariableNames[1],
                                               Config::kDefaultVariableNames[2],
                                               Config::kDefaultVariableNames[3] };
    std::unique_ptr<juce::LocalisedStrings> translations; // holds current language strings


    juce::dsp::Oversampling<float> oversampling { Config::kMaxChannels, (size_t)std::log2(Config::kOversamplingFactor), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::dsp::DryWetMixer<float> dryWetMixer;
    juce::SmoothedValue<float>    wetValue;
    juce::SmoothedValue<float>    gainCompValue;
    juce::dsp::Gain<float>        outputGain;
    juce::AudioBuffer<float>      dryBuffer;
    juce::dsp::ProcessorChain<InputRouter, InputGain, WaveShaper, SignalPolisher> chain;


    // Helper accessors for the processor chain
    InputRouter&    getInputRouter()    noexcept { return chain.get<0>(); }
    InputGain&      getInputGain()      noexcept { return chain.get<1>(); }
    WaveShaper&     getWaveShaper()     noexcept { return chain.get<2>(); }
    SignalPolisher& getPolisher()       noexcept { return chain.get<3>(); }


    juce::dsp::ProcessSpec currentSpec { Config::kDefaultSampleRate,
                                         static_cast<juce::uint32> (Config::kDefaultBlockSize),
                                         static_cast<juce::uint32> (Config::kMaxChannels) };

    void updateProcessingSpec (double sampleRate, int blockSize);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessor)
};
