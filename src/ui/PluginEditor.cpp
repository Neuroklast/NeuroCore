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
#include "StagesContentComponent.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    Localiser::getInstance().addListener(this);
    audioProcessor.addChangeListener(this);

    setResizable(true, true);
    setResizeLimits(960, 640, 1920, 1400);


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
    bypassButton->onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter(EffectParameters::dryWet))
        {
            const auto& range = p->getNormalisableRange();
            if (bypassButton->getToggleState())
            {
                mixBeforeBypass = (float) mixSlider->getValue();
                p->setValueNotifyingHost(range.convertTo0to1(0.0f));
            }
            else
            {
                p->setValueNotifyingHost(range.convertTo0to1(mixBeforeBypass));
            }
        }
    };
    addAndMakeVisible(*bypassButton);

    functionsButton = std::make_unique<juce::TextButton>(TRANS("Functions"));
    functionsButton->onClick = [this] { showFunctionsOverlay(); };
    addAndMakeVisible(*functionsButton);

    stagesButton = std::make_unique<juce::TextButton>(TRANS("StagesButton"));
    stagesButton->onClick = [this] { showStagesOverlay(); };
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
        paramComponents[i]->setMidiLearnManager(&audioProcessor.midiLearnManager);
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
        nameEditors[i]->setJustification(juce::Justification::centred);
        nameEditors[i]->setFont(juce::Font(12.f));
        nameEditors[i]->setColour(juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::surfaceHigh());
        nameEditors[i]->setColour(juce::TextEditor::textColourId, juce::Colour(0xffe8ecf4));
        nameEditors[i]->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2e3545));
        addAndMakeVisible(*nameEditors[i]);
    }

    inputGainSlider = std::make_unique<juce::Slider>();
    inputGainSlider->setSliderStyle(juce::Slider::LinearHorizontal);
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
    mixSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mixSlider->setRange(0.0, 1.0, 0.01);
    mixSlider->setValue(1.0, juce::dontSendNotification);
    mixSlider->setDoubleClickReturnValue(true, 1.0);
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
    outputGainSlider->setSliderStyle(juce::Slider::LinearHorizontal);
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

    oversamplingLabel = std::make_unique<juce::Label>("", TRANS("OversamplingLabel"));
    oversamplingLabel->setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(*oversamplingLabel);

    oversamplingBox = std::make_unique<juce::ComboBox>();
    oversamplingBox->addItemList(juce::StringArray { "1x", "2x", "4x", "8x" }, 1);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, EffectParameters::oversampling, *oversamplingBox);
    addAndMakeVisible(*oversamplingBox);


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
        syncFromProcessor();
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
            formulaInputEditor->setEditorColour(juce::TextEditor::backgroundColourId,
                                                NeuroCoreLookAndFeel::surfaceHigh());
            formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
                                                NeuroCoreLookAndFeel::accent());
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
    errorLabel->setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
    addAndMakeVisible(*errorLabel);

    inputDisplay  = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Input);
    outputDisplay = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Output);
    addAndMakeVisible(*inputDisplay);
    addAndMakeVisible(*outputDisplay);

    loudnessMeter = std::make_unique<LoudnessMeterComponent>(audioProcessor);
    addAndMakeVisible(*loudnessMeter);

    using namespace ui;
    const int pad = Config::kUiPadding;

    layoutRoot = makeColumn();
    layoutRoot->margin = pad;
    layoutRoot->innerMargin = pad;
    layoutRoot->drawBorder = false;

    auto toolbar = makeRow(0.07f);
    toolbar->innerMargin = pad;
    toolbar->addChild(makeLeaf(pluginNameLabel.get(), 2.5f));
    toolbar->addChild(makeLeaf(presetsButton.get(), 1.f));
    toolbar->addChild(makeLeaf(functionsButton.get(), 1.f));
    toolbar->addChild(makeLeaf(stagesButton.get(), 1.f));
    toolbar->addChild(makeLeaf(bypassButton.get(), 1.f));
    toolbar->addChild(makeLeaf(helpButton.get(), 0.8f));
    layoutRoot->addChild(std::move(toolbar));

    auto settingsRow = makeRow(0.055f);
    settingsRow->innerMargin = pad;
    settingsRow->addChild(makeLeaf(inputLeftButton.get(), 0.7f));
    settingsRow->addChild(makeLeaf(inputRightButton.get(), 0.7f));
    settingsRow->addChild(makeLeaf(languageLabel.get(), 0.6f));
    settingsRow->addChild(makeLeaf(languageBox.get(), 1.f));
    settingsRow->addChild(makeLeaf(oversamplingLabel.get(), 0.9f));
    settingsRow->addChild(makeLeaf(oversamplingBox.get(), 0.8f));
    settingsRow->addChild(makeLeaf(polisherLabel.get(), 0.8f));
    settingsRow->addChild(makeLeaf(polisherBox.get(), 1.f));
    settingsRow->addChild(makeLeaf(blankToggle.get(), 0.7f));
    layoutRoot->addChild(std::move(settingsRow));

    // Main body: larger left panel so the four mod knobs stay usable
    auto body = makeRow(0.56f);
    body->innerMargin = pad;

    auto leftPanel = makeColumn(3.6f);
    leftPanel->innerMargin = pad;

    auto knobGrid = makeColumn(3.0f);
    knobGrid->innerMargin = pad;
    auto knobRow1 = makeRow();
    knobRow1->innerMargin = pad;
    knobRow1->addChild(makeLeaf(paramComponents[0].get(), 1.f, 1.f));
    knobRow1->addChild(makeLeaf(paramComponents[1].get(), 1.f, 1.f));
    auto knobRow2 = makeRow();
    knobRow2->innerMargin = pad;
    knobRow2->addChild(makeLeaf(paramComponents[2].get(), 1.f, 1.f));
    knobRow2->addChild(makeLeaf(paramComponents[3].get(), 1.f, 1.f));
    knobGrid->addChild(std::move(knobRow1));
    knobGrid->addChild(std::move(knobRow2));

    auto nameRow = makeRow(0.2f);
    nameRow->innerMargin = pad;
    for (auto& ne : nameEditors)
        nameRow->addChild(makeLeaf(ne.get(), 1.f));

    leftPanel->addChild(std::move(knobGrid));
    leftPanel->addChild(std::move(nameRow));

    auto centerPanel = makeColumn(4.2f);
    centerPanel->innerMargin = pad;
    auto actionRow = makeRow(0.1f);
    actionRow->innerMargin = pad;
    actionRow->addChild(makeLeaf(editSaveButton.get(), 1.f));
    actionRow->addChild(makeLeaf(optimizeButton.get(), 1.f));
    centerPanel->addChild(std::move(actionRow));
    centerPanel->addChild(makeLeaf(formulaInputEditor.get(), 1.f));
    centerPanel->addChild(makeLeaf(errorLabel.get(), 0.08f));

    auto rightPanel = makeColumn(1.2f);
    rightPanel->innerMargin = pad;
    rightPanel->addChild(makeLeaf(loudnessMeter.get()));

    body->addChild(std::move(leftPanel));
    body->addChild(std::move(centerPanel));
    body->addChild(std::move(rightPanel));
    layoutRoot->addChild(std::move(body));

    // Full-width linear strips — taller so the track/thumb are easy to grab
    auto mixStrip = makeRow(0.13f);
    mixStrip->innerMargin = pad;
    auto makeGainColumn = [](juce::Label* label, juce::Slider* slider, juce::Label* value)
    {
        auto col = makeColumn();
        col->addChild(makeLeaf(label, 0.28f));
        col->addChild(makeLeaf(slider, 0.44f, 1.f));
        col->addChild(makeLeaf(value, 0.28f));
        return col;
    };
    mixStrip->addChild(makeGainColumn(inputGainLabel.get(), inputGainSlider.get(), inputGainValue.get()));
    mixStrip->addChild(makeGainColumn(mixLabel.get(), mixSlider.get(), mixValue.get()));
    mixStrip->addChild(makeGainColumn(outputGainLabel.get(), outputGainSlider.get(), outputGainValue.get()));
    layoutRoot->addChild(std::move(mixStrip));

    auto waveRow = makeRow(0.185f);
    waveRow->innerMargin = pad;
    waveRow->addChild(makeLeaf(inputDisplay.get(), 1.f));
    waveRow->addChild(makeLeaf(outputDisplay.get(), 1.f));
    layoutRoot->addChild(std::move(waveRow));
    updateTranslations();

    setSize(Config::kWindowWidth, Config::kWindowHeight);

}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
    attachments.clear();
    buttonAttachments.clear();
    polisherAttachment.reset();
    oversamplingAttachment.reset();
    presetOverlay.reset();
    functionsOverlay.reset();
    stagesOverlay.reset();
    validationOverlay.reset();
    audioProcessor.removeChangeListener(this);
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

void NeuroCoreAudioProcessorEditor::syncFromProcessor()
{
    // Always pull script after preset load (even if user was editing — preset wins)
    if (formulaInputEditor)
    {
        formulaInputEditor->setText(audioProcessor.getScript());
        if (editing)
        {
            editing = false;
            formulaInputEditor->setReadOnly(true);
            if (editSaveButton)
                editSaveButton->setButtonText(TRANS("EditButton"));
        }
    }

    refreshParameterControls();

    // Force slider labels / attachments to show current APVTS values
    if (inputGainSlider && inputGainValue)
    {
        const auto db = juce::Decibels::gainToDecibels((float) inputGainSlider->getValue());
        inputGainValue->setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
    }
    if (mixSlider && mixValue)
        mixValue->setText(juce::String(mixSlider->getValue() * 100.0, 1) + " %", juce::dontSendNotification);
    if (outputGainSlider && outputGainValue)
    {
        const auto db = juce::Decibels::gainToDecibels((float) outputGainSlider->getValue());
        outputGainValue->setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
    }

    if (languageBox)
    {
        const bool isDe = audioProcessor.getCurrentLanguage().startsWithIgnoreCase("de");
        languageBox->setSelectedId(isDe ? 2 : 1, juce::dontSendNotification);
    }

    if (bypassButton && mixSlider)
    {
        const float mix = (float) mixSlider->getValue();
        const bool isBypassed = mix <= 0.001f;
        bypassButton->setToggleState(isBypassed, juce::dontSendNotification);
        if (! isBypassed)
            mixBeforeBypass = mix;
    }

    if (errorLabel)
        errorLabel->setText({}, juce::dontSendNotification);

    repaint();
}

void NeuroCoreAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    syncFromProcessor();
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
    if (presetsButton)
        presetsButton->setButtonText(TRANS("Presets"));
    if (functionsButton)
        functionsButton->setButtonText(TRANS("Functions"));
    if (stagesButton)
        stagesButton->setButtonText(TRANS("StagesButton"));
    if (bypassButton)
        bypassButton->setButtonText(TRANS("Bypass"));
    if (blankToggle)
        blankToggle->setButtonText(TRANS("Blank"));
    if (polisherLabel)
        polisherLabel->setText(TRANS("PolisherLabel"), juce::dontSendNotification);
    if (helpButton)
        helpButton->setButtonText(TRANS("HelpButton"));
    if (oversamplingLabel)
        oversamplingLabel->setText(TRANS("OversamplingLabel"), juce::dontSendNotification);
}

//==============================================================================
void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bg = NeuroCoreLookAndFeel::background();
    juce::ColourGradient grad(bg.brighter(0.04f), 0.f, 0.f,
                              bg.darker(0.12f), 0.f, (float) getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    if (layoutRoot)
        ui::performLayout(*layoutRoot, getLocalBounds());

    const auto panel = NeuroCoreLookAndFeel::surface().withAlpha(0.35f);
    const auto border = NeuroCoreLookAndFeel::surfaceHigh().withAlpha(0.5f);
    const juce::Component* panelComponents[] = {
        formulaInputEditor.get(),
        loudnessMeter.get(),
        inputDisplay.get(),
        outputDisplay.get()
    };
    for (const auto* comp : panelComponents)
    {
        if (comp == nullptr || ! comp->isVisible())
            continue;
        auto r = comp->getBounds().toFloat().expanded(2.f);
        g.setColour(panel);
        g.fillRoundedRectangle(r, 8.f);
        g.setColour(border);
        g.drawRoundedRectangle(r, 8.f, 1.f);
    }
}

void NeuroCoreAudioProcessorEditor::resized()
{
    if (layoutRoot)
        ui::performLayout(*layoutRoot, getLocalBounds());
    if (pluginNameLabel)
    {
        pluginNameLabel->setFont(juce::Font(18.f, juce::Font::bold));
        pluginNameLabel->setJustificationType(juce::Justification::centredLeft);
        pluginNameLabel->setColour(juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    }
    for (auto* lbl : { inputGainLabel.get(), mixLabel.get(), outputGainLabel.get(),
                       languageLabel.get(), oversamplingLabel.get(), polisherLabel.get() })
    {
        if (lbl != nullptr)
        {
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::Font(12.f, juce::Font::bold));
            lbl->setColour(juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
        }
    }
    for (auto* lbl : { inputGainValue.get(), mixValue.get(), outputGainValue.get() })
    {
        if (lbl != nullptr)
        {
            lbl->setFont(juce::Font(13.f, juce::Font::bold));
            lbl->setColour(juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
        }
    }
    if (errorLabel)
        errorLabel->setJustificationType(juce::Justification::centredLeft);
}

bool NeuroCoreAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    auto applyUndoRedo = [this](bool isUndo) -> bool
    {
        bool ok = isUndo ? audioProcessor.undo() : audioProcessor.redo();
        if (ok)
        {
            formulaInputEditor->setText(audioProcessor.getScript());
            refreshParameterControls();
        }
        return ok;
    };

    // Ctrl+Shift+Z / Cmd+Shift+Z → Redo (check before plain Ctrl+Z)
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
        return applyUndoRedo(false);

    // Ctrl+Z / Cmd+Z → Undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
        return applyUndoRedo(true);

    // Ctrl+Y / Cmd+Y → Redo (alternative)
    if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0))
        return applyUndoRedo(false);

    return false;
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
        editing = false;
        if (formulaInputEditor)
        {
            formulaInputEditor->setReadOnly(true);
            formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
                                                juce::Colours::transparentBlack);
        }
        if (editSaveButton)
            editSaveButton->setButtonText(TRANS("EditButton"));
        if (errorLabel)
            errorLabel->setText({}, juce::dontSendNotification);

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

void NeuroCoreAudioProcessorEditor::showStagesOverlay()
{
    hideStagesOverlay();
    auto content = std::make_unique<StagesContentComponent>(audioProcessor);
    auto* ptr = content.get();

    stagesOverlay = std::make_unique<ModalOverlay>();
    stagesOverlay->setMode(OverlayMode::Closable);
    stagesOverlay->setTitle(TRANS("StagesTitle"));
    stagesOverlay->setContent(std::move(content));
    stagesOverlay->show(*this);

    ptr->onClose = [this] { stagesOverlay.reset(); };
}

void NeuroCoreAudioProcessorEditor::hideStagesOverlay()
{
    if (stagesOverlay)
        stagesOverlay.reset();
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

    validationOverlay = std::make_unique<ModalOverlay>();
    validationOverlay->setMode(OverlayMode::Blocking);
    validationOverlay->setTitle("Validating Script...");
    validationOverlay->setContent(std::move(content));
    validationOverlay->show(*this);

    ptr->onResult = [this, expr](bool stable)
    {
        validationOverlay.reset();
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
    };
}





