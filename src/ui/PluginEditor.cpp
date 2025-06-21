/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "../core/PluginProcessor.h"
#include "PluginEditor.h"
#include "../core/Config.h"
#include "../utils/FormulaHelper.h"
#include "FormulaWaveComponent.h"
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
        sliders[i]->onValueChange = [this, i]
        {
            if (valueEditors[i])
                valueEditors[i]->setText (juce::String (sliders[i]->getValue()), juce::dontSendNotification);
        };
        addAndMakeVisible (*sliders[i]);

        valueEditors[i] = std::make_unique<juce::TextEditor>();
        valueEditors[i]->setMultiLine (false);
        valueEditors[i]->setReadOnly (true);
        valueEditors[i]->setText ("0", juce::dontSendNotification);
        addAndMakeVisible (*valueEditors[i]);

        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText (audioProcessor.getVariableName(i), juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
        {
            audioProcessor.setVariableName(i, nameEditors[i]->getText());
        };
        addAndMakeVisible (*nameEditors[i]);
    }

    dryWetSlider = std::make_unique<juce::Slider>();
    dryWetSlider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    dryWetSlider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    dryWetSlider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::grey);
    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, EffectParameters::dryWet, *dryWetSlider));
    addAndMakeVisible (*dryWetSlider);

    inputLeftButton  = std::make_unique<juce::ToggleButton>(TRANS("InputLeft"));
    inputRightButton = std::make_unique<juce::ToggleButton>(TRANS("InputRight"));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, EffectParameters::useInputLeft, *inputLeftButton));
    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, EffectParameters::useInputRight, *inputRightButton));
    addAndMakeVisible(*inputLeftButton);
    addAndMakeVisible(*inputRightButton);

    formulaInputEditor = std::make_unique<InlineAutocompleteEditor>(audioProcessor);
    formulaInputEditor->setMultiLine (true, true);
    formulaInputEditor->setReturnKeyStartsNewLine (true);
    formulaInputEditor->setText (audioProcessor.getEvaluator().getFormula(), juce::dontSendNotification);
    formulaInputEditor->setReadOnly(true);
    addAndMakeVisible (*formulaInputEditor);
    audioProcessor.setFormula (formulaInputEditor->getText());



    optimizeButton = std::make_unique<juce::TextButton>(TRANS("OptimizeButton"));
    optimizeButton->onClick = [this]
    {
        juce::String info;
        auto text = formulaInputEditor->getText();
        auto opt = optimizeFormula(text, info);
        if (opt != text)
            formulaInputEditor->setText (opt, juce::dontSendNotification);
        if (info.isNotEmpty())
            errorLabel->setText(info, juce::dontSendNotification);
    };
    addAndMakeVisible (*optimizeButton);

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
                errorLabel->setText(juce::String(), juce::dontSendNotification);
            }
            else
            {
                errorLabel->setText(TRANS("CompileError") + ": " + test->getLastError(), juce::dontSendNotification);
            }
        }
    };
    addAndMakeVisible (*editSaveButton);

    errorLabel = std::make_unique<juce::Label>();
    errorLabel->setColour (juce::Label::textColourId, juce::Colours::red);
    addAndMakeVisible (*errorLabel);


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
    const int thirdWidth = Config::kMiddleColumnWidth;
    const int rowHeight  = Config::kKnobHeight + Config::kKnobSpacing;
    const int sliderSize = Config::kKnobHeight;
    const int textHeight = Config::kLabelHeight;

    for (int i = 0; i < sliders.size(); ++i)
    {
        int y = i * rowHeight;
        if (sliders[i])
            sliders[i]->setBounds (0, y, sliderSize, sliderSize);
        if (valueEditors[i])
            valueEditors[i]->setBounds (sliderSize, y, sliderSize, textHeight);
        if (nameEditors[i])
            nameEditors[i]->setBounds (sliderSize, y + textHeight, sliderSize, textHeight);
    }

    int dryWetY = static_cast<int> (sliders.size()) * rowHeight;
    if (dryWetSlider)
        dryWetSlider->setBounds (0, dryWetY, sliderSize, sliderSize);

    int buttonY = dryWetY + rowHeight;
    const int buttonHeight = textHeight;
    if (inputLeftButton)
        inputLeftButton->setBounds(0, buttonY, sliderSize, buttonHeight);
    if (inputRightButton)
        inputRightButton->setBounds(sliderSize, buttonY, sliderSize, buttonHeight);

    auto middleX = thirdWidth;
    int middleHeight = getHeight();
    int inputHeight = middleHeight * 2 / 3;

    formulaInputEditor->setBounds (middleX, 0, thirdWidth, inputHeight);
    if (editSaveButton)
        editSaveButton->setBounds (middleX, inputHeight, thirdWidth, textHeight);
    if (optimizeButton)
        optimizeButton->setBounds (middleX, inputHeight + textHeight, thirdWidth, textHeight);
    if (errorLabel)
        errorLabel->setBounds (middleX, inputHeight + 2 * textHeight, thirdWidth, textHeight);
}

