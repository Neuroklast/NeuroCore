/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class NeuroCoreAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor&);
    ~NeuroCoreAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NeuroCoreAudioProcessor& audioProcessor;

    // Left column controls
    std::array<std::unique_ptr<juce::Slider>, 4> sliders;
    std::array<std::unique_ptr<juce::TextEditor>, 4> sliderEditors;

    // Middle column editors
    std::unique_ptr<juce::TextEditor> formulaInputEditor;
    std::unique_ptr<juce::TextEditor> formulaDisplayEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessorEditor)
};
