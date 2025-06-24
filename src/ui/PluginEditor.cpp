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
#include "custom/ParameterComponent.h"
#include "../utils/Localiser.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    Localiser::getInstance().addListener(this);


    pluginNameLabel = std::make_unique<juce::Label>();
    pluginNameLabel->setText(juce::String(PLUGIN_NAME) + " v" + PLUGIN_VERSION,
                             juce::dontSendNotification);
    pluginNameLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*pluginNameLabel);

    helpButton = std::make_unique<juce::TextButton>(TRANS("HelpButton"));
    helpButton->onClick = [this]
    {

            juce::String languageSuffix = audioProcessor.getCurrentLanguage().startsWithIgnoreCase("de") ? "DE" : "EN";
            juce::File manual = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                .getSiblingFile("UserManual " + languageSuffix + ".txt");

        manual.startAsProcess();
    };
    addAndMakeVisible(*helpButton);

    blankToggle = std::make_unique<juce::ToggleButton>("Blank");
    addAndMakeVisible(*blankToggle);

    presetsButton = std::make_unique<juce::TextButton>("Presets");
    addAndMakeVisible(*presetsButton);

    bypassButton = std::make_unique<juce::ToggleButton>("Bypass");
    addAndMakeVisible(*bypassButton);

    functionsButton = std::make_unique<juce::TextButton>("Functions");
    addAndMakeVisible(*functionsButton);

    stagesButton = std::make_unique<juce::TextButton>("Stages");
    addAndMakeVisible(*stagesButton);

    languageLabel = std::make_unique<juce::Label>();
    languageLabel->setMinimumHorizontalScale(1.0f);
    languageLabel->setText(TRANS("LanguageLabel"), juce::dontSendNotification);
    addAndMakeVisible(*languageLabel);

    languageBox = std::make_unique<juce::ComboBox>();
    languageBox->addItem("English", 1);
    languageBox->addItem("Deutsch", 2);
    languageBox->onChange = [this]
    {
        auto id = languageBox->getSelectedId();
        audioProcessor.loadLanguage(id == 2 ? "de" : "en");
    };
    languageBox->setSelectedId(audioProcessor.getCurrentLanguage().startsWithIgnoreCase("de") ? 2 : 1, juce::dontSendNotification);
    addAndMakeVisible(*languageBox);
   


    const float startAngle = juce::MathConstants<float>::pi * 4.0f / 3.0f;
    const float endAngle   = juce::MathConstants<float>::pi * 8.0f / 3.0f;

    for (int i = 0; i < paramComponents.size(); ++i)
    {

        auto paramId = juce::String::charToString(static_cast<juce_wchar>('a' + i));
        paramComponents[i] = std::make_unique<ui::ParameterComponent>(audioProcessor.apvts,
                                                                    paramId,
                                                                    audioProcessor.getVariableName(i));
        addAndMakeVisible(*paramComponents[i]);
        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText(audioProcessor.getVariableName(i), juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
        {
            audioProcessor.setVariableName(i, nameEditors[i]->getText());
            if (paramComponents[i])
                paramComponents[i]->setAliasName(nameEditors[i]->getText());
        };
        addAndMakeVisible(*nameEditors[i]);
    }


    inputGainSlider = std::make_unique<juce::Slider>();
    inputGainSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider->setRotaryParameters(startAngle, endAngle, true);
    inputGainSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider->setTooltip(TRANS("InputGainLabel"));
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
    mixSlider->setTooltip(TRANS("MixLabel"));
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
    outputGainSlider->setTooltip(TRANS("OutputGainLabel"));
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
    inputGainLabel->setMinimumHorizontalScale(1.0f);
    mixLabel        = std::make_unique<juce::Label>("", TRANS("MixLabel"));
    mixLabel->setMinimumHorizontalScale(1.0f);
    outputGainLabel = std::make_unique<juce::Label>("", TRANS("OutputGainLabel"));
    outputGainLabel->setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(*inputGainLabel);
    addAndMakeVisible(*mixLabel);
    addAndMakeVisible(*outputGainLabel);

    polisherLabel = std::make_unique<juce::Label>("", TRANS("PolisherLabel"));
    polisherLabel->setMinimumHorizontalScale(1.0f);
    addAndMakeVisible (*polisherLabel);

    polisherBox = std::make_unique<juce::ComboBox>();
    polisherBox->addItemList (juce::StringArray { "None", "Hard Clip", "Limiter" }, 1);
    polisherAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, EffectParameters::polisherMode, *polisherBox);
    addAndMakeVisible (*polisherBox);


    inputGainValue  = std::make_unique<juce::Label>();
    mixValue        = std::make_unique<juce::Label>();
    outputGainValue = std::make_unique<juce::Label>();
    for (auto* l : { inputGainValue.get(), mixValue.get(), outputGainValue.get() })
    {
        l->setJustificationType(juce::Justification::centred);
        l->setMinimumHorizontalScale(1.0f);
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
    formulaInputEditor->setText(audioProcessor.getScript(), juce::dontSendNotification);
    formulaInputEditor->setReadOnly(true);
    formulaInputEditor->setColour(juce::TextEditor::backgroundColourId, Colours::black);
    // hide caret while editor is read only
    formulaInputEditor->setColour(juce::CaretComponent::caretColourId,
        Colours::black);
    addAndMakeVisible(*formulaInputEditor);
    {
        juce::String err;
        audioProcessor.setFormula(formulaInputEditor->getText(), err);
        refreshParameterControls();
    }


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
            formulaInputEditor->setColour(juce::TextEditor::backgroundColourId, Colours::black);
            // show caret when editor becomes editable
            formulaInputEditor->setColour(juce::CaretComponent::caretColourId,
                                          juce::Colours::black);
            editSaveButton->setButtonText(TRANS("SaveButton"));
        }
        else
        {
            auto text = formulaInputEditor->getText();
            juce::String err;
            if (audioProcessor.setFormula(text, err))
            {
                formulaInputEditor->setReadOnly(true);
                
                // hide caret again after saving
                formulaInputEditor->setColour(juce::CaretComponent::caretColourId,
                                              juce::Colours::transparentBlack);
                editSaveButton->setButtonText(TRANS("EditButton"));
                editing = false;
               
                errorLabel->setText({}, juce::dontSendNotification);
                refreshParameterControls();
            }
            else
            {
                errorLabel->setText(err, juce::dontSendNotification);
            }
        }
    };
    addAndMakeVisible(*editSaveButton);

    errorLabel = std::make_unique<juce::Label>();
    errorLabel->setMinimumHorizontalScale(1.0f);
    errorLabel->setColour(juce::Label::textColourId, juce::Colours::red);
    addAndMakeVisible(*errorLabel);

    inputDisplay  = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Input);
    outputDisplay = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Output);
    addAndMakeVisible(*inputDisplay);
    addAndMakeVisible(*outputDisplay);

    loudnessMeter = std::make_unique<LoudnessMeterComponent>(audioProcessor);
    addAndMakeVisible(*loudnessMeter);

    updateTranslations();

    setSize(Config::kWindowWidth, Config::kWindowHeight);

}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
    attachments.clear();
    buttonAttachments.clear();
    polisherAttachment.reset();
    Localiser::getInstance().removeListener(this);
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

void NeuroCoreAudioProcessorEditor::refreshParameterControls()
{
    for (int i = 0; i < paramComponents.size(); ++i)
    {
        if (nameEditors[i])
            nameEditors[i]->setText(audioProcessor.getVariableName(i), juce::dontSendNotification);
        if (paramComponents[i])
            paramComponents[i]->setAliasName(audioProcessor.getVariableName(i));
        bool active = audioProcessor.isParameterActive(i);
        if (paramComponents[i])
            paramComponents[i]->setEnabled(active);
        if (nameEditors[i])
            nameEditors[i]->setEnabled(active);
    }
   
}

void NeuroCoreAudioProcessorEditor::updateTranslations()
{
    languageLabel->setText(TRANS("LanguageLabel"), juce::dontSendNotification);
    inputGainLabel->setText(TRANS("InputGainLabel"), juce::dontSendNotification);
    mixLabel->setText(TRANS("MixLabel"), juce::dontSendNotification);
    outputGainLabel->setText(TRANS("OutputGainLabel"), juce::dontSendNotification);
    inputLeftButton->setButtonText(TRANS("InputLeft"));
    inputRightButton->setButtonText(TRANS("InputRight"));
    optimizeButton->setButtonText(TRANS("OptimizeButton"));
    editSaveButton->setButtonText(editing ? TRANS("SaveButton") : TRANS("EditButton"));
    if (polisherLabel)
        polisherLabel->setText(TRANS("PolisherLabel"), juce::dontSendNotification);
    if (helpButton)
        helpButton->setButtonText(TRANS("HelpButton"));
}

//==============================================================================
void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

}

void NeuroCoreAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().toFloat();

    juce::Grid grid;
    grid.rowGap    = juce::Grid::Px(Config::kUiPadding);
    grid.columnGap = juce::Grid::Px(Config::kUiPadding);

    for (int i = 0; i < Config::kGridRows; ++i)
        grid.templateRows.add(juce::Grid::TrackInfo(1_fr));

    for (int i = 0; i < Config::kGridColumns; ++i)
        grid.templateColumns.add(juce::Grid::TrackInfo(1_fr));

    auto add = [&](juce::Component* comp, int rStart, int rEnd, int cStart, int cEnd)
    {
        if (!comp) return;
        grid.items.add(juce::GridItem(*comp).withArea(rStart + 1, cStart + 1,
                                                     rEnd + 1, cEnd + 1));
    };

    add(pluginNameLabel.get(), 0, 2, 0, 2);
    add(helpButton.get(),      0, 2, 2, 4);
    add(inputLeftButton.get(), 0, 2, 4, 5);
    add(inputRightButton.get(),0, 2, 5, 6);
    add(blankToggle.get(),     0, 2, 6, 7);
    add(presetsButton.get(),   0, 2, 7, 9);
    add(bypassButton.get(),    0, 2, 9,12);

    add(formulaInputEditor.get(), 2, 8, 0, 8);

    add(editSaveButton.get(),   2, 3, 8,12);
    add(optimizeButton.get(),   3, 4, 8,12);
    add(functionsButton.get(),  4, 5, 8,12);
    add(stagesButton.get(),     5, 6, 8,12);

    add(loudnessMeter.get(),   6,14, 9,12);

    add(paramComponents[0].get(), 8,11,0,3);
    add(paramComponents[1].get(), 8,11,3,6);
    add(paramComponents[2].get(), 8,11,6,9);
    add(paramComponents[3].get(), 8,11,9,12);
    add(inputDisplay.get(),      8,11,9,12);

    add(inputGainSlider.get(), 11,14,0,4);
    add(mixSlider.get(),        11,14,4,8);
    add(outputGainSlider.get(), 11,14,8,12);

    add(outputDisplay.get(),    14,16,3,9);

    grid.performLayout(bounds.reduced(Config::kUiPadding).toNearestInt());

    clampChildrenToBounds();
}

void NeuroCoreAudioProcessorEditor::clampChildrenToBounds()
{
    auto bounds = getLocalBounds();
    for (int i = 0; i < getNumChildComponents(); ++i)
    {
        if (auto* c = getChildComponent(i))
            c->setBounds(c->getBounds().getIntersection(bounds));
    }
}

