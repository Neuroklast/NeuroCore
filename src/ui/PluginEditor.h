/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include "PluginLookAndFeel.h"
#include "../utils/FormulaHelper.h"
#include "WaveformDisplayComponent.h"
#include "InlineAutocompleteEditor.h"


//==============================================================================
/**
*/
class NeuroCoreAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor&);
    ~NeuroCoreAudioProcessorEditor() override;

    juce::String getFormulaText() const;
    void setFormulaText(const juce::String& text);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NeuroCoreAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    // Left column controls
    std::array<std::unique_ptr<juce::Slider>, 4> sliders;
    std::array<std::unique_ptr<juce::TextEditor>, 4> nameEditors;
    std::array<juce::Colour, 4> sliderColours;
    std::unique_ptr<juce::Slider> inputGainSlider;
    std::unique_ptr<juce::Slider> mixSlider;
    std::unique_ptr<juce::Slider> outputGainSlider;
    std::unique_ptr<juce::Label>  inputGainLabel;
    std::unique_ptr<juce::Label>  mixLabel;
    std::unique_ptr<juce::Label>  outputGainLabel;

    // Middle column editors
    std::unique_ptr<InlineAutocompleteEditor> formulaInputEditor;
    NeuroCoreLookAndFeel lookAndFeel;
    std::unique_ptr<WaveformDisplayComponent> inputDisplay;
    std::unique_ptr<WaveformDisplayComponent> outputDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessorEditor)
};



