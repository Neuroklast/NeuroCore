/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <array>
#include "ExpressionEvaluator.h"
#include "PresetManager.h"
#include "InputGain.h"
#include "WaveShaper.h"
#include "SignalPolisher.h"
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
    const ExpressionEvaluator& getEvaluator() const noexcept { return evaluator; }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Evaluates current formula for a single sample value.
    float evaluateFormula (float x);

private:
    //==============================================================================
    ExpressionEvaluator evaluator;
    std::array<std::atomic<float>, 4> parameterValues{};
    std::array<juce::String, 4> variableNames{ "a", "b", "c", "d" };
    std::unique_ptr<juce::LocalisedStrings> translations; // holds current language strings

    InputGain inputGain;
    WaveShaper waveShaper{ &evaluator };
    SignalPolisher polisher;
    juce::dsp::Oversampling<float> oversampling { Config::kMaxChannels, (size_t)std::log2(Config::kOversamplingFactor), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::dsp::ProcessorChain<InputGain, WaveShaper, SignalPolisher> chain;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessor)
};
