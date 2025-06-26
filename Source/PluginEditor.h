/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "PluginProcessor.h"
#include "FormulaDisplayComponent.h"
#include "FormulaHelper.h"
#include "FormulaWaveComponent.h"
#include "Config.h"


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
    bool keyPressed (const juce::KeyPress& key) override;

private:
    NeuroCoreAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    // Left column controls
    std::array<std::unique_ptr<juce::Slider>, 4> sliders;
    std::array<std::unique_ptr<juce::TextEditor>, 4> valueEditors;
    std::array<std::unique_ptr<juce::TextEditor>, 4> nameEditors;
    std::array<juce::Colour, 4> sliderColours;

    // Middle column editors
    std::unique_ptr<juce::TextEditor> formulaInputEditor;
    std::unique_ptr<FormulaDisplayComponent> formulaDisplay;
    std::unique_ptr<juce::TextButton> optimizeButton;

    // Language selection
    std::unique_ptr<juce::ComboBox> languageBox;
    juce::StringArray languageCodes;

    void showAutocomplete();

    std::unique_ptr<FormulaWaveComponent> waveComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessorEditor)
};



