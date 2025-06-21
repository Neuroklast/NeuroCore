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
#include "FormulaDisplayComponent.h"
#include "PluginLookAndFeel.h"
#include "InlineAutocompleteEditor.h"
#include "../core/EffectParameters.h"
#include "../utils/ExpressionEvaluator.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
   

    static const juce::Colour defaultColours[4] = {
        juce::Colours::red, juce::Colours::green,
        juce::Colours::blue, juce::Colours::yellow };

    const float startAngle = juce::MathConstants<float>::pi * 4.0f / 3.0f;
    const float endAngle   = juce::MathConstants<float>::pi * 8.0f / 3.0f;

    for (int i = 0; i < sliders.size(); ++i)
    {
        sliders[i] = std::make_unique<juce::Slider>();
        sliders[i]->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i]->setRotaryParameters (startAngle, endAngle, true);
        sliders[i]->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sliders[i]->setColour (juce::Slider::rotarySliderFillColourId, defaultColours[i]);
        sliderColours[i] = defaultColours[i];
        auto paramId = juce::String::charToString (static_cast<juce_wchar>('a' + i));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramId, *sliders[i]));
        sliders[i]->onValueChange = [this, i]
        {
            if (valueLabels[i])
                valueLabels[i]->setText (juce::String (sliders[i]->getValue(), 2), juce::dontSendNotification);
        };
        addAndMakeVisible (*sliders[i]);

        valueLabels[i] = std::make_unique<juce::Label>();
        valueLabels[i]->setJustificationType (juce::Justification::centred);
        valueLabels[i]->setText ("0", juce::dontSendNotification);
        addAndMakeVisible (*valueLabels[i]);

        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText (audioProcessor.getVariableName(i), juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
        {
            audioProcessor.setVariableName(i, nameEditors[i]->getText());
            formulaDisplay->setVariableColours(audioProcessor.getVariableNames(), sliderColours);
        };
        addAndMakeVisible (*nameEditors[i]);
    }

    inputGainSlider = std::make_unique<juce::Slider>();
    inputGainSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider->setRotaryParameters(startAngle, endAngle, true);
    inputGainSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, EffectParameters::inputGain, *inputGainSlider));
    inputGainSlider->onValueChange = [this]
    {
        if (inputGainValue)
        {
            auto db = juce::Decibels::gainToDecibels((float)inputGainSlider->getValue());
            inputGainValue->setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        }
    };
    addAndMakeVisible(*inputGainSlider);

    mixSlider = std::make_unique<juce::Slider>();
    mixSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider->setRotaryParameters(startAngle, endAngle, true);
    mixSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, EffectParameters::dryWet, *mixSlider));
    mixSlider->onValueChange = [this]
    {
        if (mixValue)
            mixValue->setText(juce::String(mixSlider->getValue() * 100.0f, 1) + " %", juce::dontSendNotification);
    };
    addAndMakeVisible(*mixSlider);

    outputGainSlider = std::make_unique<juce::Slider>();
    outputGainSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider->setRotaryParameters(startAngle, endAngle, true);
    outputGainSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, EffectParameters::outputGain, *outputGainSlider));
    outputGainSlider->onValueChange = [this]
    {
        if (outputGainValue)
        {
            auto db = juce::Decibels::gainToDecibels((float)outputGainSlider->getValue());
            outputGainValue->setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        }
    };
    addAndMakeVisible(*outputGainSlider);

    inputGainLabel  = std::make_unique<juce::Label>("", "INPUT GAIN");
    mixLabel        = std::make_unique<juce::Label>("", "MIX");
    outputGainLabel = std::make_unique<juce::Label>("", "OUTPUT GAIN");
    addAndMakeVisible(*inputGainLabel);
    addAndMakeVisible(*mixLabel);
    addAndMakeVisible(*outputGainLabel);

    inputGainValue  = std::make_unique<juce::Label>();
    mixValue        = std::make_unique<juce::Label>();
    outputGainValue = std::make_unique<juce::Label>();
    for (auto* l : { inputGainValue.get(), mixValue.get(), outputGainValue.get() })
    {
        l->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*l);
    }

    inputLeftButton  = std::make_unique<juce::ToggleButton>(TRANS("InputLeft"));
    inputRightButton = std::make_unique<juce::ToggleButton>(TRANS("InputRight"));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, EffectParameters::useInputLeft, *inputLeftButton));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, EffectParameters::useInputRight, *inputRightButton));
    addAndMakeVisible(*inputLeftButton);
    addAndMakeVisible(*inputRightButton);

    formulaInputEditor = std::make_unique<InlineAutocompleteEditor>(audioProcessor);
    formulaInputEditor->setMultiLine(true, true);
    formulaInputEditor->setReturnKeyStartsNewLine(true);
    formulaInputEditor->setText(audioProcessor.getEvaluator().getFormula(), juce::dontSendNotification);
    formulaInputEditor->setReadOnly(true);
    formulaInputEditor->onTextChange = [this]
    {
        if (formulaDisplay)
            formulaDisplay->setFormula(formulaInputEditor->getText());
    };
    addAndMakeVisible(*formulaInputEditor);
    audioProcessor.setFormula(formulaInputEditor->getText());

    formulaDisplay = std::make_unique<FormulaDisplayComponent>();
    formulaDisplay->setVariableColours(audioProcessor.getVariableNames(), sliderColours);
    formulaDisplay->setFormula(formulaInputEditor->getText());
    addAndMakeVisible(*formulaDisplay);

    optimizeButton = std::make_unique<juce::TextButton>(TRANS("OptimizeButton"));
    optimizeButton->onClick = [this]
    {
        juce::String info;
        auto text = formulaInputEditor->getText();
        auto opt  = optimizeFormula(text, info);
        if (opt != text)
            formulaInputEditor->setText(opt, juce::dontSendNotification);
        if (info.isNotEmpty())
            errorLabel->setText(info, juce::dontSendNotification);
    };
    addAndMakeVisible(*optimizeButton);

    editSaveButton = std::make_unique<juce::TextButton>(TRANS("EditButton"));
    editSaveButton->onClick = [this]
    {
        if (! editing)
        {
            editing = true;
            formulaInputEditor->setReadOnly(false);
            editSaveButton->setButtonText(TRANS("SaveButton"));
        }
        else
        {
            auto text = formulaInputEditor->getText();
            auto test = std::make_shared<ExpressionEvaluator>();
            if (test->parseFormula(text.toStdString()))
            {
                audioProcessor.setFormula(text);
                formulaInputEditor->setReadOnly(true);
                editSaveButton->setButtonText(TRANS("EditButton"));
                editing = false;
                formulaDisplay->setFormula(text);
                errorLabel->setText(juce::String(), juce::dontSendNotification);
            }
            else
            {
                errorLabel->setText(TRANS("CompileError") + ": " + test->getLastError(), juce::dontSendNotification);
            }
        }
    };
    addAndMakeVisible(*editSaveButton);

    errorLabel = std::make_unique<juce::Label>();
    errorLabel->setColour(juce::Label::textColourId, juce::Colours::red);
    addAndMakeVisible(*errorLabel);

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
    const int knobSize    = 90;
    const int labelHeight = 24;
    const int valueHeight = 20;

    if (formulaInputEditor)
        formulaInputEditor->setBounds(400, 30, 400, 120);
    if (formulaDisplay)
        formulaDisplay->setBounds(400, 150, 400, labelHeight);
    if (editSaveButton)
        editSaveButton->setBounds(400, 180, 400, labelHeight);
    if (optimizeButton)
        optimizeButton->setBounds(400, 210, 400, labelHeight);
    if (errorLabel)
        errorLabel->setBounds(400, 240, 400, labelHeight);

    const int yPos = 280;
    const int xStart = 415;
    for (int i = 0; i < sliders.size(); ++i)
    {
        if (sliders[i])
            sliders[i]->setBounds(xStart + i * 100, yPos, knobSize, knobSize);
        if (valueLabels[i])
            valueLabels[i]->setBounds(xStart + i * 100, yPos + knobSize, knobSize, valueHeight);
        if (nameEditors[i])
            nameEditors[i]->setBounds(xStart + i * 100, yPos + knobSize + valueHeight, knobSize, labelHeight);
    }

    int gainY = yPos + knobSize + valueHeight + labelHeight + 20;
    if (inputGainSlider)
        inputGainSlider->setBounds(460, gainY, 80, 80);
    if (mixSlider)
        mixSlider->setBounds(560, gainY - 20, 120, 120);
    if (outputGainSlider)
        outputGainSlider->setBounds(700, gainY, 80, 80);
    if (inputGainLabel)
        inputGainLabel->setBounds(460, gainY + 80, 80, labelHeight);
    if (mixLabel)
        mixLabel->setBounds(560, gainY + 100, 120, labelHeight);
    if (outputGainLabel)
        outputGainLabel->setBounds(700, gainY + 80, 80, labelHeight);
    if (inputGainValue)
        inputGainValue->setBounds(460, gainY + 80 + labelHeight, 80, valueHeight);
    if (mixValue)
        mixValue->setBounds(560, gainY + 100 + labelHeight, 120, valueHeight);
    if (outputGainValue)
        outputGainValue->setBounds(700, gainY + 80 + labelHeight, 80, valueHeight);
    if (inputLeftButton)
        inputLeftButton->setBounds(20, gainY, 100, labelHeight);
    if (inputRightButton)
        inputRightButton->setBounds(20, gainY + labelHeight, 100, labelHeight);

    if (inputDisplay)
        inputDisplay->setBounds(40, gainY, 320, 120);
    if (outputDisplay)
        outputDisplay->setBounds(840, gainY, 320, 120);
}

