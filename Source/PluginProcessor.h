/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "ExpressionEvaluator.h"

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Evaluates current formula for a single sample value.
    float evaluateFormula (float x);

private:
    //==============================================================================
    ExpressionEvaluator evaluator;
    std::array<juce::String, 4> variableNames{ "a", "b", "c", "d" };
    float modPhase = 0.0f; // phase accumulator for sine LFO
    juce::SmoothedValue<float> smoothedA;
    juce::SmoothedValue<float> smoothedB;
    juce::SmoothedValue<float> smoothedC;
    juce::SmoothedValue<float> smoothedD;
    juce::SmoothedValue<float> smoothedModFreq;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessor)
};
