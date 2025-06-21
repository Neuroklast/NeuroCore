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
#include "LoudnessMeterComponent.h"
#include "../core/EffectParameters.h"
#include "../utils/ExpressionEvaluator.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    languageLabel = std::make_unique<juce::Label>();
    languageLabel->setText(TRANS("LanguageLabel"), juce::dontSendNotification);
    addAndMakeVisible(*languageLabel);

    languageBox = std::make_unique<juce::ComboBox>();
    languageBox->addItem("English", 1);
    languageBox->addItem("Deutsch", 2);
    languageBox->onChange = [this]
    {
        auto id = languageBox->getSelectedId();
        audioProcessor.loadLanguage(id == 2 ? "de" : "en");
        languageLabel->setText(TRANS("LanguageLabel"), juce::dontSendNotification);
        inputGainLabel->setText(TRANS("InputGainLabel"), juce::dontSendNotification);
        mixLabel->setText(TRANS("MixLabel"), juce::dontSendNotification);
        outputGainLabel->setText(TRANS("OutputGainLabel"), juce::dontSendNotification);
        inputLeftButton->setButtonText(TRANS("InputLeft"));
        inputRightButton->setButtonText(TRANS("InputRight"));
        optimizeButton->setButtonText(TRANS("OptimizeButton"));
        editSaveButton->setButtonText(editing ? TRANS("SaveButton") : TRANS("EditButton"));
    };
    languageBox->setSelectedId(audioProcessor.getCurrentLanguage().startsWithIgnoreCase("de") ? 2 : 1, juce::dontSendNotification);
    addAndMakeVisible(*languageBox);
   

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

    inputGainLabel  = std::make_unique<juce::Label>("", TRANS("InputGainLabel"));
    mixLabel        = std::make_unique<juce::Label>("", TRANS("MixLabel"));
    outputGainLabel = std::make_unique<juce::Label>("", TRANS("OutputGainLabel"));
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

    loudnessMeter = std::make_unique<LoudnessMeterComponent>(audioProcessor);
    addAndMakeVisible(*loudnessMeter);


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
    const int knobSize    = Config::kKnobSize;
    const int labelHeight = Config::kLabelHeight;
    const int valueHeight = Config::kValueFieldHeight;

    if (languageLabel)
        languageLabel->setBounds(Config::kLanguageBoxX,
                                 Config::kLanguageBoxY,
                                 Config::kLanguageLabelWidth,
                                 labelHeight);
    if (languageBox)
        languageBox->setBounds(Config::kLanguageBoxX + Config::kLanguageLabelWidth + 4,
                               Config::kLanguageBoxY,
                               Config::kLanguageBoxWidth,
                               labelHeight);

    if (formulaInputEditor)
        formulaInputEditor->setBounds(Config::kFormulaEditorX, Config::kFormulaEditorY,
                                     Config::kFormulaEditorWidth, Config::kFormulaEditorHeight);
    if (formulaDisplay)
        formulaDisplay->setBounds(Config::kFormulaEditorX, Config::kFormulaDisplayY,
                                  Config::kFormulaEditorWidth, labelHeight);
    if (editSaveButton)
        editSaveButton->setBounds(Config::kFormulaEditorX, Config::kEditButtonY,
                                  Config::kFormulaEditorWidth, labelHeight);
    if (optimizeButton)
        optimizeButton->setBounds(Config::kFormulaEditorX, Config::kOptimizeButtonY,
                                  Config::kFormulaEditorWidth, labelHeight);
    if (errorLabel)
        errorLabel->setBounds(Config::kFormulaEditorX, Config::kErrorLabelY,
                              Config::kFormulaEditorWidth, labelHeight);

    const int yPos = Config::kKnobRowY;
    const int xStart = Config::kKnobRowXStart;
    for (int i = 0; i < sliders.size(); ++i)
    {
        if (sliders[i])
            sliders[i]->setBounds(xStart + i * Config::kKnobRowSpacing, yPos, knobSize, knobSize);
        if (valueLabels[i])
            valueLabels[i]->setBounds(xStart + i * Config::kKnobRowSpacing, yPos + knobSize, knobSize, valueHeight);
        if (nameEditors[i])
            nameEditors[i]->setBounds(xStart + i * Config::kKnobRowSpacing, yPos + knobSize + valueHeight, knobSize, labelHeight);
    }

    int gainY = yPos + knobSize + valueHeight + labelHeight + Config::kGainSectionGap;
    if (inputGainSlider)
        inputGainSlider->setBounds(Config::kInputGainX, gainY, Config::kGainKnobSize, Config::kGainKnobSize);
    if (mixSlider)
        mixSlider->setBounds(Config::kMixX, gainY + Config::kMixYOffset, Config::kMixKnobSize, Config::kMixKnobSize);
    if (outputGainSlider)
        outputGainSlider->setBounds(Config::kOutputGainX, gainY, Config::kGainKnobSize, Config::kGainKnobSize);
    if (inputGainLabel)
        inputGainLabel->setBounds(Config::kInputGainX, gainY + Config::kGainKnobSize, Config::kGainKnobSize, labelHeight);
    if (mixLabel)
        mixLabel->setBounds(Config::kMixX, gainY + Config::kMixKnobSize + Config::kMixLabelYOffset, Config::kMixKnobSize, labelHeight);
    if (outputGainLabel)
        outputGainLabel->setBounds(Config::kOutputGainX, gainY + Config::kGainKnobSize, Config::kGainKnobSize, labelHeight);
    if (inputGainValue)
        inputGainValue->setBounds(Config::kInputGainX, gainY + Config::kGainKnobSize + labelHeight, Config::kGainKnobSize, valueHeight);
    if (mixValue)
        mixValue->setBounds(Config::kMixX, gainY + Config::kMixKnobSize + Config::kMixLabelYOffset + labelHeight, Config::kMixKnobSize, valueHeight);
    if (outputGainValue)
        outputGainValue->setBounds(Config::kOutputGainX, gainY + Config::kGainKnobSize + labelHeight, Config::kGainKnobSize, valueHeight);
    if (inputLeftButton)
        inputLeftButton->setBounds(Config::kInputButtonX, gainY, Config::kInputButtonWidth, labelHeight);
    if (inputRightButton)
        inputRightButton->setBounds(Config::kInputButtonX, gainY + labelHeight, Config::kInputButtonWidth, labelHeight);

    if (inputDisplay)
        inputDisplay->setBounds(Config::kWaveDisplayXLeft, gainY, Config::kWaveDisplayWidth, Config::kWaveDisplayHeight);
    if (outputDisplay)
        outputDisplay->setBounds(Config::kWaveDisplayXRight, gainY, Config::kWaveDisplayWidth, Config::kWaveDisplayHeight);
    if (loudnessMeter)
        loudnessMeter->setBounds(Config::kLoudnessMeterX, gainY + Config::kWaveDisplayHeight + 20,
                                 Config::kLoudnessMeterWidth, Config::kLoudnessMeterHeight);
}

