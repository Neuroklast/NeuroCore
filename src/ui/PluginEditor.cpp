/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "PluginEditor.h"
#include "../core/Config.h"
#include "../utils/FormulaHelper.h"
#include "WaveformDisplayComponent.h"
#include "FormulaDisplayComponent.h"
#include "PluginLookAndFeel.h"
#include "DslTerminalEditor.h"
#include "LoudnessMeterComponent.h"
#include "../core/EffectParameters.h"
#include "custom/ParameterComponent.h"
#include "../utils/Localiser.h"
#include "WeightedLayout.h"
#include "ModalOverlay.h"
#include "ValidationContentComponent.h"
#include "PresetContentComponent.h"
#include "FunctionsContentComponent.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
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

    blankToggle = std::make_unique<juce::ToggleButton>(TRANS("Blank"));
    addAndMakeVisible(*blankToggle);

    presetsButton = std::make_unique<juce::TextButton>(TRANS("Presets"));
    presetsButton->onClick = [this] { showPresetOverlay(); };
    addAndMakeVisible(*presetsButton);

    bypassButton = std::make_unique<juce::ToggleButton>(TRANS("Bypass"));
    addAndMakeVisible(*bypassButton);

    functionsButton = std::make_unique<juce::TextButton>(TRANS("Functions"));
    functionsButton->onClick = [this] { showFunctionsOverlay(); };
    addAndMakeVisible(*functionsButton);

    stagesButton = std::make_unique<juce::TextButton>(TRANS("Stages"));
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

    for (int i = 0; i < (int)paramComponents.size(); ++i)
    {
        // ID a,b,c,d
        auto paramId = juce::String::charToString(static_cast<juce_wchar>('a' + i));

        // 2.1) ParameterComponent bauen und sichtbar machen
        paramComponents[i] = std::make_unique<ui::ParameterComponent>(
            audioProcessor.apvts,
            paramId,
            audioProcessor.getVariableName(i));
        addAndMakeVisible(*paramComponents[i]);

        // 2.2) TextEditor für Alias-Name
        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText(audioProcessor.getVariableName(i),
            juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
            {
                auto newName = nameEditors[i]->getText();
                audioProcessor.setVariableName(i, newName);
                paramComponents[i]->setAliasName(newName);
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
    mixSlider->setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider->setRotaryParameters(startAngle, endAngle, true);
    mixSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 30);
    mixSlider->setRange(0.0, 1.0, 0.01);
	mixSlider->setValue(0.5, juce::dontSendNotification);
	mixSlider->setDoubleClickReturnValue(true, 0.5);
	mixSlider->setSkewFactorFromMidPoint(0.5);
	mixSlider->setScrollWheelEnabled(true);
	



	

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

    formulaInputEditor = std::make_unique<DslTerminalEditor>(audioProcessor);
    formulaInputEditor->setText(audioProcessor.getScript());
    formulaInputEditor->setOpaque(true);
    formulaInputEditor->setReadOnly(true);
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
            formulaInputEditor->setText(opt);
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
            formulaInputEditor->setEditorColour(juce::TextEditor::backgroundColourId, Colours::black);
            formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
                                              juce::Colours::black);
            editSaveButton->setButtonText(TRANS("SaveButton"));
        }
        else
        {
            auto text = formulaInputEditor->getText();
            validateAndOverlay(text);
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

    using namespace ui;
    layoutRoot = makeColumn();
    layoutRoot->margin = Config::kUiPadding;
	layoutRoot->innerMargin = Config::kUiPadding;
	layoutRoot->aspectRatio = 1.618f; // Golden ratio for a nice layout

	layoutRoot->drawBorder = true;


	auto header = makeRow(0.1f);
	header->innerMargin = Config::kUiPadding;
	header->margin = Config::kUiPadding;
	header->drawBorder = false;
	

    
    
    header->addChild(makeLeaf(pluginNameLabel.get(), 2.0f));
    header->addChild(makeLeaf(helpButton.get(), 1.0f));
    header->addChild(makeLeaf(inputLeftButton.get(), 1.0f));
    header->addChild(makeLeaf(inputRightButton.get(), 1.0f));
    header->addChild(makeLeaf(bypassButton.get(), 1.0f));
    header->addChild(makeLeaf(languageLabel.get(), 1.0f));
    header->addChild(makeLeaf(languageBox.get(), 1.0f));
    layoutRoot->addChild(std::move(header));



    auto body = makeRow();
    body->innerMargin = Config::kUiPadding;
	
	//
    // body->aspectRatio = 1.618f; // Golden ratio for a nice layout

	auto left = makeColumn(5.f);
    auto editor = makeRow();
	auto formulaEditor = makeColumn(6.f);
    auto paramKnobs = makeColumn(1.f);
    auto buttons = makeColumn(1.f);
	
    for (auto& pc : paramComponents)
        paramKnobs->addChild(makeLeaf(pc.get(), 1.f, 1.f));

	buttons->innerMargin = Config::kUiPadding;
    buttons->drawBorder = false;
	buttons->margin = Config::kUiPadding;
   
    buttons->addChild(makeLeaf(editSaveButton.get(), 0.3f, 2.f));
    buttons->addChild(makeLeaf(optimizeButton.get(), 0.3f, 2.f));
    buttons->addChild(makeLeaf(functionsButton.get(), 0.3f, 2.f));
    buttons->addChild(makeLeaf(stagesButton.get(), 0.3f, 2.f));
    buttons->addChild(makeLeaf(presetsButton.get(), 0.3f, 2.f));

	formulaEditor->addChild(makeLeaf(formulaInputEditor.get()));

    editor->addChild(std::move(paramKnobs));
    editor->addChild(std::move(formulaEditor));
	editor->addChild(std::move(buttons));

	left->addChild(std::move(editor));





    auto right = makeColumn(1.f);
	right->innerMargin = Config::kUiPadding;
	right->drawBorder = false;
	right->margin = Config::kUiPadding;

    right->addChild(makeLeaf(loudnessMeter.get()));
    









	auto mixKnobs = makeRow(0.2);
	mixKnobs->addChild(makeLeaf(inputGainSlider.get()));
	mixKnobs->addChild(makeLeaf(mixSlider.get()));
	mixKnobs->addChild(makeLeaf(outputGainSlider.get()));


	auto wavemeter = makeRow(0.8f, 0, true);
	wavemeter->drawBorder = true;
	wavemeter->innerMargin = Config::kUiPadding;
	wavemeter->margin = Config::kUiPadding;
    wavemeter->addChild(makeLeaf(inputDisplay.get()));
    wavemeter->addChild(makeLeaf(outputDisplay.get()));

	




    body->addChild(std::move(left));

    body->addChild(std::move(right));

  

    layoutRoot->addChild(std::move(body));
	layoutRoot->addChild(std::move(mixKnobs));
	layoutRoot->addChild(std::move(wavemeter));

    updateTranslations();


    


    setSize(Config::kWindowWidth, Config::kWindowHeight);

}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
    attachments.clear();
    buttonAttachments.clear();
    polisherAttachment.reset();
    presetOverlay.reset();
    functionsOverlay.reset();
    validationOverlay.reset();
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
        formulaInputEditor->setText (text);
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
    g.fillAll (Colours::black);
    layoutRoot->layoutAndDraw(g, getLocalBounds());

}

void NeuroCoreAudioProcessorEditor::resized()
{
    if (layoutRoot)
        ui::performLayout(*layoutRoot, getLocalBounds());
    if (pluginNameLabel)
    {
        pluginNameLabel->setFont(juce::Font(16.0f, juce::Font::bold));
        pluginNameLabel->setJustificationType(juce::Justification::centred);
    }
}

void NeuroCoreAudioProcessorEditor::showPresetOverlay()
{
    hidePresetOverlay();
    auto content = std::make_unique<PresetContentComponent>(audioProcessor, lookAndFeel);
    auto* ptr = content.get();

    presetOverlay = std::make_unique<ModalOverlay>();
    presetOverlay->setMode(OverlayMode::Closable);
    presetOverlay->setTitle(TRANS("Presets"));
    presetOverlay->setContent(std::move(content));
    presetOverlay->show(*this);

    ptr->onPresetSelected = [this](int idx)
    {
        audioProcessor.loadPreset(idx);
        presetOverlay.reset();
    };
    ptr->onClose = [this] { presetOverlay.reset(); };
}

void NeuroCoreAudioProcessorEditor::showFunctionsOverlay()
{
    hideFunctionsOverlay();
    auto content = std::make_unique<FunctionsContentComponent>(audioProcessor);
    auto* ptr = content.get();

    functionsOverlay = std::make_unique<ModalOverlay>();
    functionsOverlay->setMode(OverlayMode::Closable);
    functionsOverlay->setTitle(TRANS("Functions"));
    functionsOverlay->setContent(std::move(content));
    functionsOverlay->show(*this);

    ptr->onInsert = [this](const juce::String& text)
    {
        formulaInputEditor->insertTextAtCaret(text);
    };
    ptr->onClose = [this]{ functionsOverlay.reset(); };
}

void NeuroCoreAudioProcessorEditor::hideFunctionsOverlay()
{
    if (functionsOverlay)
        functionsOverlay.reset();
}
void NeuroCoreAudioProcessorEditor::hidePresetOverlay()
{
    if (presetOverlay)
        presetOverlay.reset();
}

void NeuroCoreAudioProcessorEditor::validateAndOverlay(const juce::String& expr)
{
    validationOverlay.reset();

    auto content = std::make_unique<ValidationContentComponent>(audioProcessor, expr);
    auto* ptr = content.get();
    
    // Set to persist after save for analysis review
    ptr->setPersistAfterSave(true);

    validationOverlay = std::make_unique<ModalOverlay>();
    validationOverlay->setMode(OverlayMode::Blocking);
    validationOverlay->setTitle("Validating Script...");
    validationOverlay->setContent(std::move(content));
    validationOverlay->show(*this);

    ptr->onResult = [this, expr](ValidationContentComponent::ValidationResult result)
    {
        switch (result)
        {
            case ValidationContentComponent::ValidationResult::Success:
            case ValidationContentComponent::ValidationResult::UserProceed:
            {
                // Apply the code
                juce::String err;
                if (audioProcessor.setFormula(expr, err))
                {
                    formulaInputEditor->setReadOnly(true);
                    formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
                                                      juce::Colours::transparentBlack);
                    editSaveButton->setButtonText(TRANS("EditButton"));
                    editing = false;
                    errorLabel->setText({}, juce::dontSendNotification);
                    refreshParameterControls();
                    
                    // For successful validation, keep overlay open to show analysis
                    if (result == ValidationContentComponent::ValidationResult::Success) {
                        validationOverlay->setTitle("Validation Complete - Analysis Results");
                        validationOverlay->setMode(OverlayMode::Closable);
                        // Don't reset the overlay, let user close it manually
                        return;
                    }
                }
                else
                {
                    errorLabel->setText(err, juce::dontSendNotification);
                }
                validationOverlay.reset();
                break;
            }
            
            case ValidationContentComponent::ValidationResult::UserCancel:
                // User chose to go back to editor - don't apply changes
                validationOverlay.reset();
                // Keep in editing mode
                break;
                
            case ValidationContentComponent::ValidationResult::Stable:
                // Legacy compatibility - treat as success
                juce::String err;
                if (audioProcessor.setFormula(expr, err))
                {
                    formulaInputEditor->setReadOnly(true);
                    formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
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
                validationOverlay.reset();
                break;
        }
    };
}





