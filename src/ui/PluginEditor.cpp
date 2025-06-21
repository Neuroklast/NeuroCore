/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "../core/PluginProcessor.h"
#include "PluginEditor.h"
#include "../core/Config.h"
#include "../utils/FormulaHelper.h"
#include "WaveformDisplayComponent.h"
#include "PluginLookAndFeel.h"
#include "InlineAutocompleteEditor.h"
#include "../core/EffectParameters.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
   

    static const juce::Colour defaultColours[4] = {
        juce::Colours::red, juce::Colours::green,
        juce::Colours::blue, juce::Colours::yellow };

    for (int i = 0; i < sliders.size(); ++i)
    {
        sliders[i] = std::make_unique<juce::Slider>();
        sliders[i]->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i]->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sliders[i]->setColour (juce::Slider::rotarySliderFillColourId, defaultColours[i]);
        sliderColours[i] = defaultColours[i];
        auto paramId = juce::String::charToString (static_cast<juce_wchar>('a' + i));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramId, *sliders[i]));
        addAndMakeVisible (*sliders[i]);

        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText (audioProcessor.getVariableName(i), juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
        {
            audioProcessor.setVariableName(i, nameEditors[i]->getText());
        };
        addAndMakeVisible (*nameEditors[i]);
    }

    inputGainSlider = std::make_unique<juce::Slider>();
    inputGainSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, EffectParameters::inputGain, *inputGainSlider));
    addAndMakeVisible(*inputGainSlider);

    mixSlider = std::make_unique<juce::Slider>();
    mixSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, EffectParameters::dryWet, *mixSlider));
    addAndMakeVisible(*mixSlider);

    outputGainSlider = std::make_unique<juce::Slider>();
    outputGainSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, EffectParameters::outputGain, *outputGainSlider));
    addAndMakeVisible(*outputGainSlider);

    inputGainLabel  = std::make_unique<juce::Label>("", "INPUT GAIN");
    mixLabel        = std::make_unique<juce::Label>("", "MIX");
    outputGainLabel = std::make_unique<juce::Label>("", "OUTPUT GAIN");
    addAndMakeVisible(*inputGainLabel);
    addAndMakeVisible(*mixLabel);
    addAndMakeVisible(*outputGainLabel);

    formulaInputEditor = std::make_unique<InlineAutocompleteEditor>(audioProcessor);
    formulaInputEditor->setMultiLine (true, true);
    formulaInputEditor->setReturnKeyStartsNewLine (true);
    formulaInputEditor->setText (audioProcessor.getEvaluator().getFormula(), juce::dontSendNotification);
    formulaInputEditor->setReadOnly(true);
    addAndMakeVisible (*formulaInputEditor);
    audioProcessor.setFormula (formulaInputEditor->getText());



    formulaInputEditor = std::make_unique<InlineAutocompleteEditor>(audioProcessor);
    formulaInputEditor->setMultiLine(true, true);
    formulaInputEditor->setReturnKeyStartsNewLine(true);
    formulaInputEditor->setText(audioProcessor.getEvaluator().getFormula(), juce::dontSendNotification);
    addAndMakeVisible(*formulaInputEditor);
    audioProcessor.setFormula(formulaInputEditor->getText());

    inputDisplay  = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Input);
    outputDisplay = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Output);
    addAndMakeVisible(*inputDisplay);
    addAndMakeVisible(*outputDisplay);


    setSize(Config::kWindowWidth, Config::kWindowHeight);

}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

juce::String NeuroCoreAudioProcessorEditor::getFormulaText() const
{
    return formulaInputEditor ? formulaInputEditor->getText() : juce::String();
}

void NeuroCoreAudioProcessorEditor::setFormulaText(const juce::String& text)
{
    if (formulaInputEditor)
        formulaInputEditor->setText (text, juce::dontSendNotification);
}

//==============================================================================
void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

}

void NeuroCoreAudioProcessorEditor::resized()
{
    if (formulaInputEditor)
        formulaInputEditor->setBounds(400, 30, 400, 150);

    const int knobSize = 90;
    const int labelHeight = 24;
    const int yPos = 200;

    const int xStart = 415;
    for (int i = 0; i < sliders.size(); ++i)
    {
        if (sliders[i])
            sliders[i]->setBounds(xStart + i * 100, yPos, knobSize, knobSize);
        if (nameEditors[i])
            nameEditors[i]->setBounds(xStart + i * 100, 295, knobSize, labelHeight);
    }

    if (inputGainSlider)
        inputGainSlider->setBounds(460, 420, 80, 80);
    if (mixSlider)
        mixSlider->setBounds(560, 400, 120, 120);
    if (outputGainSlider)
        outputGainSlider->setBounds(700, 420, 80, 80);
    if (inputGainLabel)
        inputGainLabel->setBounds(460, 510, 80, labelHeight);
    if (mixLabel)
        mixLabel->setBounds(560, 525, 120, labelHeight);
    if (outputGainLabel)
        outputGainLabel->setBounds(700, 510, 80, labelHeight);

    if (inputDisplay)
        inputDisplay->setBounds(40, 600, 320, 120);
    if (outputDisplay)
        outputDisplay->setBounds(840, 640, 320, 120);
}

