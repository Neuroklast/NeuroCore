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
}

//==============================================================================
void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

}

void NeuroCoreAudioProcessorEditor::resized()
{
    const int pad = Config::kUiPadding;
    auto bounds   = getLocalBounds().reduced(pad).toFloat();

    const float col = bounds.getWidth()  / Config::kGridColumns;
    const float row = bounds.getHeight() / Config::kGridRows;

    juce::FlexBox flex;
    flex.flexWrap      = juce::FlexBox::Wrap::wrap;
    flex.alignContent  = juce::FlexBox::AlignContent::stretch;
    flex.alignItems    = juce::FlexBox::AlignItems::center;
    flex.justifyContent= juce::FlexBox::JustifyContent::flexStart;

    auto addItem = [&](juce::Component* comp, const Config::GridArea& area)
    {
        if (comp == nullptr)
            return;

        juce::FlexItem item;
        item.associatedComponent = comp; // Associate the component
        item.width = col * area.columnSpan; // Set width
        item.height = row * area.rowSpan; // Set height
        item.order     = area.row * Config::kGridColumns + area.column;
        item.margin    = pad;

        if (dynamic_cast<juce::Slider*>(comp))
        {
            item.minHeight = Config::kRotaryDiameterMin;
            item.minWidth  = Config::kRotaryDiameterMin;
        }

        if (dynamic_cast<ui::ParameterComponent*>(comp))
        {
            item.minHeight = Config::kRotaryDiameterMin + 105;
            item.minWidth  = Config::kRotaryDiameterMin + 60;
        }

        flex.items.add(std::move(item));
    };

    addItem(languageLabel.get(),    Config::kAreaLanguageLabel);
    addItem(languageBox.get(),      Config::kAreaLanguageBox);
    addItem(inputLeftButton.get(),  Config::kAreaInputLeftButton);
    addItem(inputRightButton.get(), Config::kAreaInputRightButton);
    addItem(polisherLabel.get(),    Config::kAreaPolisherLabel);
    addItem(polisherBox.get(),      Config::kAreaPolisherBox);
    addItem(formulaInputEditor.get(), Config::kAreaFormulaEditor);
    addItem(editSaveButton.get(),     Config::kAreaEditButton);
    addItem(optimizeButton.get(),     Config::kAreaOptimizeButton);
    addItem(errorLabel.get(),         Config::kAreaErrorLabel);

    for (int i = 0; i < paramComponents.size(); ++i)
    {
        addItem(paramComponents[i].get(), Config::kAreaKnobs[i]);
        addItem(nameEditors[i].get(),     Config::kAreaKnobNames[i]);
    }

    addItem(inputGainSlider.get(),   Config::kAreaInputGainSlider);
    addItem(mixSlider.get(),         Config::kAreaMixSlider);
    addItem(outputGainSlider.get(),  Config::kAreaOutputGainSlider);

    addItem(inputGainLabel.get(),    Config::kAreaInputGainLabel);
    addItem(mixLabel.get(),          Config::kAreaMixLabel);
    addItem(outputGainLabel.get(),   Config::kAreaOutputGainLabel);

    addItem(inputGainValue.get(),    Config::kAreaInputGainValue);
    addItem(mixValue.get(),          Config::kAreaMixValue);
    addItem(outputGainValue.get(),   Config::kAreaOutputGainValue);

    addItem(inputDisplay.get(),      Config::kAreaInputDisplay);
    addItem(outputDisplay.get(),     Config::kAreaOutputDisplay);
    addItem(loudnessMeter.get(),     Config::kAreaLoudnessMeter);

    flex.performLayout(bounds);

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

