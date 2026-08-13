/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <BinaryData.h>
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
#include "OptimizeContentComponent.h"
#include "FunctionsContentComponent.h"
#include "StagesContentComponent.h"
#include "HelpContentComponent.h"
#include "StandaloneAudioSettings.h"
#include "../utils/FormulaQuality.h"
#include "fx/CyberFxTypes.h"
#include "fx/CyberClip.h"


//==============================================================================
NeuroCoreAudioProcessorEditor::NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    Localiser::getInstance().addListener(this);
    audioProcessor.addChangeListener(this);

    setResizable(true, true);
    setResizeLimits(960, 640, 1920, 1400);
    setOpaque(true);

    backdrop = std::make_unique<CyberBackdropComponent> (cyberDirector);

    brandLockup = std::make_unique<BrandLockup> (
        lookAndFeel.getNkLogo(),
        juce::String (PLUGIN_NAME),
        juce::String ("v") + PLUGIN_VERSION);
    logoGlitch.director = &cyberDirector;
    brandLockup->addMouseListener (&logoGlitch, false);
    addAndMakeVisible (*brandLockup);

    statusBarLabel = std::make_unique<juce::Label>();
    statusBarLabel->setJustificationType(juce::Justification::centredLeft);
    statusBarLabel->setColour(juce::Label::textColourId, NeuroCoreLookAndFeel::accent().withAlpha(0.75f));
    statusBarLabel->setFont(NeuroCoreLookAndFeel::monoFont(11.f));
    statusBarLabel->setMinimumHorizontalScale(0.55f);
    addAndMakeVisible(*statusBarLabel);

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        audioSettingsButton = std::make_unique<juce::TextButton> ("AUDIO");
        audioSettingsButton->setTooltip ("Sample rate / device (Standalone). Host plugins follow the DAW.");
        audioSettingsButton->onClick = [] { tryOpenStandaloneAudioSettings(); };
        addAndMakeVisible (*audioSettingsButton);
    }

    helpButton = std::make_unique<juce::TextButton>(TRANS("HelpButton"));
    helpButton->onClick = [this]
    {
        juce::String body;
        // Prefer embedded manual (always offline), then loose resource file
        if (BinaryData::UserManual_en_txtSize > 0)
            body = juce::String::fromUTF8 (BinaryData::UserManual_en_txt, BinaryData::UserManual_en_txtSize);
        if (body.isEmpty())
        {
            auto f = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                         .getSiblingFile ("resources")
                         .getChildFile ("UserManual_en.txt");
            if (f.existsAsFile())
                body = f.loadFileAsString();
        }
        if (body.isEmpty())
            body = "NeuroCore Help\n\nThe manual could not be loaded. Try closing Help and opening it again.";

        auto viewer = std::make_unique<HelpContentComponent> (body);

        validationOverlay = std::make_unique<ModalOverlay>();
        validationOverlay->setMode (OverlayMode::Closable);
        validationOverlay->setTitle ("Help / User Manual");
        validationOverlay->setPreferredContentSize (juce::jmin (getWidth() - 24, 1100),
                                                    juce::jmin (getHeight() - 24, 740));
        validationOverlay->setContent (std::move (viewer));
        applyOverlayMotion (*validationOverlay);
        validationOverlay->show (*this);
        syncGlCover();
        validationOverlay->onClose = [this] { validationOverlay.reset(); syncGlCover(); };
    };
    addAndMakeVisible(*helpButton);

    licenseButton = std::make_unique<juce::TextButton> ("License");
    licenseButton->setTooltip ("Import a signed .lic file");
    licenseButton->onClick = [this]
    {
        auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        licenseChooser = std::make_unique<juce::FileChooser> (
            "Import NeuroCore license", start, "*.lic");
        constexpr int flags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;
        licenseChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto src = fc.getResult();
            if (! src.existsAsFile())
                return;
            if (audioProcessor.importProductLicense (src))
            {
                if (errorLabel != nullptr)
                    errorLabel->setText ("Licensed: " + audioProcessor.licensedEmail(),
                                         juce::dontSendNotification);
            }
            else if (errorLabel != nullptr)
            {
                auto msg = audioProcessor.licenseError();
                if (msg.isEmpty())
                    msg = "License file was rejected.";
                errorLabel->setText (msg, juce::dontSendNotification);
            }
        });
    };
    addAndMakeVisible (*licenseButton);

    editorFontLabel = std::make_unique<juce::Label> ("", "Text");
    editorFontLabel->setMinimumHorizontalScale (1.0f);
    editorFontLabel->setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (*editorFontLabel);

    editorFontMinusButton = std::make_unique<juce::TextButton> ("-");
    editorFontPlusButton  = std::make_unique<juce::TextButton> ("+");
    editorFontSizeLabel   = std::make_unique<juce::Label> ("", juce::String ((int) Config::kDefaultEditorFontPt));
    editorFontSizeLabel->setJustificationType (juce::Justification::centred);
    editorFontSizeLabel->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::brightText());
    editorFontMinusButton->setTooltip ("Decrease formula text size");
    editorFontPlusButton->setTooltip ("Increase formula text size");
    editorFontMinusButton->onClick = [this]
    {
        applyEditorFontSize (editorFontHeight - Config::kEditorFontStepPt);
    };
    editorFontPlusButton->onClick = [this]
    {
        applyEditorFontSize (editorFontHeight + Config::kEditorFontStepPt);
    };
    addAndMakeVisible (*editorFontMinusButton);
    addAndMakeVisible (*editorFontPlusButton);
    addAndMakeVisible (*editorFontSizeLabel);

    presetsButton = std::make_unique<juce::TextButton>(TRANS("Presets"));
    presetsButton->onClick = [this] { showPresetOverlay(); };
    addAndMakeVisible(*presetsButton);

    // Cyber OS bypass — TextButton with toggle state, painted by LookAndFeel angular style
    bypassButton = std::make_unique<juce::TextButton>("BYPASS");
    bypassButton->setClickingTogglesState (true);
    bypassButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a0505));
    bypassButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff1a1a));
    bypassButton->setColour (juce::TextButton::textColourOffId, NeuroCoreLookAndFeel::accent());
    bypassButton->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    bypassButton->onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter(EffectParameters::dryWet))
        {
            const auto& range = p->getNormalisableRange();
            if (bypassButton->getToggleState())
            {
                mixBeforeBypass = (float) mixSlider->getValue();
                if (mixBeforeBypass < 0.001f)
                    mixBeforeBypass = 1.0f;
                p->setValueNotifyingHost(range.convertTo0to1(0.0f));
                bypassButton->setButtonText ("BYPASSED");
            }
            else
            {
                p->setValueNotifyingHost(range.convertTo0to1(mixBeforeBypass));
                bypassButton->setButtonText ("BYPASS");
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

    // Language fixed to English (UI switch removed — brand default)


    const float startAngle = juce::MathConstants<float>::pi * 4.0f / 3.0f;
    const float endAngle   = juce::MathConstants<float>::pi * 8.0f / 3.0f;

    for (int i = 0; i < (int)paramComponents.size(); ++i)
    {
        // ID a,b,c,d
        auto paramId = juce::String::charToString(static_cast<juce_wchar>('a' + i));
        const auto knobCol = FormulaDisplayComponent::knobColour(i);

        // 2.1) ParameterComponent bauen und sichtbar machen
        paramComponents[i] = std::make_unique<ui::ParameterComponent>(
            audioProcessor.apvts,
            paramId,
            audioProcessor.getVariableName(i));
        paramComponents[i]->setMidiLearnManager(&audioProcessor.midiLearnManager);
        paramComponents[i]->setAccentColour(knobCol);
        addAndMakeVisible(*paramComponents[i]);

        // 2.2) TextEditor für Alias-Name — outline matches knob colour
        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText(audioProcessor.getVariableName(i),
            juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
            {
                auto newName = nameEditors[i]->getText();
                audioProcessor.setVariableName(i, newName);
                paramComponents[i]->setAliasName(newName);
                updateLiveFormulaView();
            };
        nameEditors[i]->setJustification(juce::Justification::centred);
        nameEditors[i]->setFont(juce::Font(12.f));
        nameEditors[i]->setColour(juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::surfaceHigh());
        nameEditors[i]->setColour(juce::TextEditor::textColourId, knobCol);
        nameEditors[i]->setColour(juce::TextEditor::outlineColourId, knobCol.withAlpha(0.65f));
        nameEditors[i]->setColour(juce::TextEditor::focusedOutlineColourId, knobCol);
        addAndMakeVisible(*nameEditors[i]);
    }

    mixSlider = std::make_unique<CyberMixSlider>();
    mixSlider->setValue (1.0, juce::dontSendNotification);
    mixSlider->onGlitchPulse = [this] (float strength, int seed)
    {
        cyberDirector.triggerGlitch (strength, seed);
    };
    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, EffectParameters::dryWet, *mixSlider));
    mixSlider->onValueChange = [this]
    {
        if (mixValue)
            mixValue->setText (juce::String (mixSlider->getValue() * 100.0f, 0) + "%",
                               juce::dontSendNotification);
    };
    addAndMakeVisible (*mixSlider);

    if (auto* p = audioProcessor.apvts.getParameter (EffectParameters::outputGain))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (1.0f));

    mixLabel = std::make_unique<juce::Label> ("", "MIX");
    mixLabel->setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (*mixLabel);

    currentPresetLabel = std::make_unique<juce::Label>("currentPreset", juce::String());
    currentPresetLabel->setJustificationType (juce::Justification::centredLeft);
    currentPresetLabel->setMinimumHorizontalScale (0.7f);
    currentPresetLabel->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::brightText());
    currentPresetLabel->setColour (juce::Label::backgroundColourId,
                                   NeuroCoreLookAndFeel::accent().withAlpha (0.18f));
    currentPresetLabel->setColour (juce::Label::outlineColourId, NeuroCoreLookAndFeel::accent());
    currentPresetLabel->setFont (NeuroCoreLookAndFeel::brandFont (14.f, true));
    currentPresetLabel->setBorderSize ({ 6, 12, 6, 12 });
    currentPresetLabel->setTooltip ("Current preset - click Presets to browse / save");
    addAndMakeVisible (*currentPresetLabel);

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


    mixValue = std::make_unique<juce::Label>();
    mixValue->setJustificationType (juce::Justification::centred);
    mixValue->setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (*mixValue);

    inputChannelSwitch = std::make_unique<InputChannelSwitch> (audioProcessor.apvts);
    addAndMakeVisible (*inputChannelSwitch);

    formulaInputEditor = std::make_unique<DslTerminalEditor>(audioProcessor);
    formulaInputEditor->setText(audioProcessor.getScript());
    formulaInputEditor->setOpaque(true);
    formulaInputEditor->setReadOnly(true);
    formulaInputEditor->setVisible(false);
    addChildComponent(*formulaInputEditor);

    formulaLiveDisplay = std::make_unique<FormulaDisplayComponent>();
    {
        std::array<juce::String, Config::kNumUserParams> names;
        std::array<juce::Colour, Config::kNumUserParams> colours;
        for (int i = 0; i < Config::kNumUserParams; ++i)
        {
            names[(size_t) i]   = audioProcessor.getVariableName(i);
            colours[(size_t) i] = FormulaDisplayComponent::knobColour(i);
        }
        formulaLiveDisplay->setVariableColours(names, colours);
        formulaLiveDisplay->setFormula(audioProcessor.getScript());
    }
    addAndMakeVisible(*formulaLiveDisplay);
    applyEditorFontSize (editorFontHeight);
    syncFromProcessor();

    optimizeButton = std::make_unique<juce::TextButton>(TRANS("OptimizeButton"));
    optimizeButton->onClick = [this]
    {
        juce::String text = editing && formulaInputEditor != nullptr
                                ? formulaInputEditor->getText()
                                : audioProcessor.getScript();
        if (text.trim().isEmpty() && formulaInputEditor != nullptr)
            text = formulaInputEditor->getText();

        auto content = std::make_unique<OptimizeContentComponent> (audioProcessor, text);
        auto* ptr = content.get();
        // Reuse validation overlay slot for optimizer panel
        validationOverlay = std::make_unique<ModalOverlay>();
        validationOverlay->setMode (OverlayMode::Closable);
        validationOverlay->setTitle ("Optimizer");
        validationOverlay->setPreferredContentSize (juce::jmin (getWidth() - 30, 960),
                                                    juce::jmin (getHeight() - 30, 640));
        validationOverlay->setContent (std::move (content));
        validationOverlay->show (*this);
        syncGlCover();
        validationOverlay->onClose = [this] { validationOverlay.reset(); syncGlCover(); };
        ptr->onClose = [this] { validationOverlay.reset(); };
        ptr->onApply = [this] (const juce::String& opt)
        {
            if (! editing)
                setFormulaEditMode (true);
            if (formulaInputEditor)
            {
                formulaInputEditor->setText (opt);
                formulaInputEditor->setReadOnly (false);
            }
            juce::String err;
            audioProcessor.applyFormula (opt, err, false);
            updateLiveFormulaView();
            if (errorLabel)
            {
                errorLabel->setColour (juce::Label::textColourId, juce::Colour (0xff7dcea0));
                errorLabel->setText ("Optimized formula applied", juce::dontSendNotification);
            }
            validationOverlay.reset();
        };
    };
    addAndMakeVisible(*optimizeButton);

    // Copy works in view mode (live formula) and edit mode
    copyFormulaButton = std::make_unique<juce::TextButton> (TRANS ("CopyButton") == "CopyButton"
                                                                ? "Copy" : TRANS ("CopyButton"));
    copyFormulaButton->setTooltip ("Copy formula to clipboard (works outside edit mode)");
    copyFormulaButton->onClick = [this]
    {
        juce::String text;
        if (editing && formulaInputEditor != nullptr)
            text = formulaInputEditor->getText();
        else
            text = audioProcessor.getScript();
        if (text.isEmpty() && formulaInputEditor != nullptr)
            text = formulaInputEditor->getText();
        juce::SystemClipboard::copyTextToClipboard (text);
        if (errorLabel != nullptr)
            errorLabel->setText (text.isEmpty()
                                     ? "Nothing to copy"
                                     : (TRANS ("CopiedToClipboard") == "CopiedToClipboard"
                                            ? "Copied to clipboard"
                                            : TRANS ("CopiedToClipboard")),
                                 juce::dontSendNotification);
    };
    addAndMakeVisible (*copyFormulaButton);

    editSaveButton = std::make_unique<juce::TextButton>(TRANS("EditButton"));
    editSaveButton->onClick = [this]
    {
        if (! editing)
            setFormulaEditMode(true);
        else
            validateAndOverlay(formulaInputEditor->getText());
    };
    addAndMakeVisible(*editSaveButton);

    // Timer started after layout (cyber OS + live formula)

    errorLabel = std::make_unique<juce::Label>();
    errorLabel->setMinimumHorizontalScale(1.0f);
    errorLabel->setColour(juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    errorLabel->setFont (NeuroCoreLookAndFeel::monoFont (12.f));
    addAndMakeVisible(*errorLabel);

    inputDisplay  = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Input);
    outputDisplay = std::make_unique<WaveformDisplayComponent>(audioProcessor, WaveformDisplayComponent::Type::Output);
    // Signal path: dimmer red for input, full brand red for processed output
    inputDisplay->lineColour  = NeuroCoreLookAndFeel::accent().withAlpha (0.85f);
    outputDisplay->lineColour = NeuroCoreLookAndFeel::accent();
    inputDisplay->lineThickness  = 1.4f;
    outputDisplay->lineThickness = 1.6f;
    inputDisplay->setOpaque (true);
    outputDisplay->setOpaque (true);
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
    toolbar->addChild(makeLeaf(brandLockup.get(), 1.85f));
    toolbar->addChild(makeLeaf(presetsButton.get(), 0.9f));
    // Current preset chip next to Presets — high visibility
    toolbar->addChild(makeLeaf(currentPresetLabel.get(), 2.4f));
    toolbar->addChild(makeLeaf(functionsButton.get(), 0.9f));
    toolbar->addChild(makeLeaf(stagesButton.get(), 0.9f));
    toolbar->addChild(makeLeaf(bypassButton.get(), 0.9f));
    toolbar->addChild(makeLeaf(licenseButton.get(), 0.75f));
    toolbar->addChild(makeLeaf(helpButton.get(), 0.7f));
    layoutRoot->addChild(std::move(toolbar));

    auto settingsRow = makeRow(0.055f);
    settingsRow->innerMargin = pad;
    auto addChrome = [&] (ui::LayoutNode& parent, juce::Component* c, float w)
    {
        auto n = makeLeaf (c, w);
        n->maxHeight = Config::kChromeControlHeight;
        parent.addChild (std::move (n));
    };
    // Same column weights as knobs / editor / meter so L/BOTH/R and Text-/+
    // sit on those gutters.
    addChrome (*settingsRow, inputChannelSwitch.get(), Config::kBodyKnobsWeight);

    auto midChrome = makeRow (Config::kBodyEditorWeight);
    midChrome->innerMargin = pad;
    midChrome->addChild (makeLeaf (oversamplingLabel.get(), 1.0f));
    addChrome (*midChrome, oversamplingBox.get(), 0.9f);
    midChrome->addChild (makeLeaf (polisherLabel.get(), 0.9f));
    addChrome (*midChrome, polisherBox.get(), 1.1f);
    settingsRow->addChild (std::move (midChrome));

    auto fontChrome = makeRow (Config::kBodyMeterWeight);
    fontChrome->innerMargin = 3;
    fontChrome->addChild (makeLeaf (editorFontLabel.get(), 0.9f));
    addChrome (*fontChrome, editorFontMinusButton.get(), 0.55f);
    fontChrome->addChild (makeLeaf (editorFontSizeLabel.get(), 0.7f));
    addChrome (*fontChrome, editorFontPlusButton.get(), 0.55f);
    settingsRow->addChild (std::move (fontChrome));
    layoutRoot->addChild(std::move(settingsRow));

    // Main body: 6 knobs (2x3) left + larger formula editor + meter
    auto body = makeRow(0.52f);
    body->innerMargin = pad;

    auto leftPanel = makeColumn(Config::kBodyKnobsWeight);
    leftPanel->innerMargin = pad;

    auto knobGrid = makeColumn(2.6f);
    knobGrid->innerMargin = 4;
    // 3 rows x 2 knobs (a..f) — tight gap so leftover width goes to the editor
    for (int row = 0; row < 3; ++row)
    {
        auto knobRow = makeRow();
        knobRow->innerMargin = 4;
        for (int col = 0; col < 2; ++col)
        {
            const int idx = row * 2 + col;
            if (idx < Config::kNumUserParams)
                knobRow->addChild (makeLeaf (paramComponents[(size_t) idx].get(), 1.f, 1.f));
        }
        knobGrid->addChild (std::move (knobRow));
    }

    auto nameRow = makeRow(0.14f);
    nameRow->innerMargin = pad;
    for (int i = 0; i < Config::kNumUserParams; ++i)
        nameRow->addChild (makeLeaf (nameEditors[(size_t) i].get(), 1.f));

    leftPanel->addChild(std::move(knobGrid));
    leftPanel->addChild(std::move(nameRow));

    auto centerPanel = makeColumn(Config::kBodyEditorWeight);
    centerPanel->innerMargin = pad;
    auto actionRow = makeRow(0.09f);
    actionRow->innerMargin = pad;
    actionRow->addChild(makeLeaf(editSaveButton.get(), 1.f));
    actionRow->addChild(makeLeaf(copyFormulaButton.get(), 0.85f));
    actionRow->addChild(makeLeaf(optimizeButton.get(), 1.f));
    centerPanel->addChild(std::move(actionRow));

    // Live annotated formula (default) - editor shares bounds when editing
    centerPanel->addChild(makeLeaf(formulaLiveDisplay.get(), 1.f));
    centerPanel->addChild(makeLeaf(errorLabel.get(), 0.08f));

    auto rightPanel = makeColumn(Config::kBodyMeterWeight);
    rightPanel->innerMargin = pad;
    rightPanel->addChild(makeLeaf(loudnessMeter.get()));

    body->addChild(std::move(leftPanel));
    body->addChild(std::move(centerPanel));
    body->addChild(std::move(rightPanel));
    layoutRoot->addChild(std::move(body));

    auto mixStrip = makeRow (0.07f);
    mixStrip->innerMargin = pad;
    mixStrip->minHeight = 36;
    mixStrip->addChild (makeLeaf (mixLabel.get(), 0.7f));
    mixStrip->addChild (makeLeaf (mixSlider.get(), 6.2f, 0.f));
    mixStrip->addChild (makeLeaf (mixValue.get(), 0.8f));
    layoutRoot->addChild (std::move (mixStrip));

    // Waveforms need real height — minHeight enforced so they never collapse
    auto waveRow = makeRow(0.20f);
    waveRow->innerMargin = pad;
    waveRow->minHeight = 120;
    {
        auto inLeaf = makeLeaf(inputDisplay.get(), 1.f);
        inLeaf->minHeight = 110;
        auto outLeaf = makeLeaf(outputDisplay.get(), 1.f);
        outLeaf->minHeight = 110;
        waveRow->addChild(std::move(inLeaf));
        waveRow->addChild(std::move(outLeaf));
    }
    layoutRoot->addChild(std::move(waveRow));

    // Live DSP status (SR / OS / MIX / LIM / LUFS)
    auto statusRow = makeRow(0.045f);
    statusRow->innerMargin = pad;
    statusRow->minHeight = 22;
    if (audioSettingsButton)
        statusRow->addChild(makeLeaf(audioSettingsButton.get(), 0.7f));
    statusRow->addChild(makeLeaf(statusBarLabel.get(), 5.5f));
    layoutRoot->addChild(std::move(statusRow));

    updateTranslations();
    updateStatusBar();

    setSize(Config::kWindowWidth, Config::kWindowHeight);
    startTimerHz (30); // live formula values track knobs smoothly
}

NeuroCoreAudioProcessorEditor::~NeuroCoreAudioProcessorEditor()
{
    stopTimer();
    attachments.clear();
    polisherAttachment.reset();
    oversamplingAttachment.reset();
    assembleVblank = {};
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
    updateLiveFormulaView();
}

void NeuroCoreAudioProcessorEditor::applyEditorFontSize (float heightPt)
{
    editorFontHeight = juce::jlimit (Config::kMinEditorFontPt, Config::kMaxEditorFontPt, heightPt);
    if (formulaInputEditor != nullptr)
        formulaInputEditor->setFontHeight (editorFontHeight);
    if (formulaLiveDisplay != nullptr)
        formulaLiveDisplay->setFontHeight (editorFontHeight);
    if (editorFontSizeLabel != nullptr)
        editorFontSizeLabel->setText (juce::String ((int) std::round (editorFontHeight)),
                                      juce::dontSendNotification);
}

void NeuroCoreAudioProcessorEditor::setFormulaEditMode(bool shouldEdit)
{
    editing = shouldEdit;
    if (formulaInputEditor)
    {
        formulaInputEditor->setReadOnly(! shouldEdit);
        formulaInputEditor->setVisible(shouldEdit);
        if (shouldEdit)
        {
            formulaInputEditor->setEditorColour(juce::TextEditor::backgroundColourId,
                                                NeuroCoreLookAndFeel::background());
            formulaInputEditor->setEditorColour(juce::TextEditor::textColourId,
                                                NeuroCoreLookAndFeel::brightText());
            formulaInputEditor->setEditorColour(juce::TextEditor::highlightColourId,
                                                NeuroCoreLookAndFeel::accent().withAlpha (0.35f));
            formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
                                                NeuroCoreLookAndFeel::accent());
            formulaInputEditor->setEditorColour(juce::TextEditor::outlineColourId,
                                                NeuroCoreLookAndFeel::accent().withAlpha (0.5f));
            // Editor overlays the live display bounds
            if (formulaLiveDisplay)
                formulaInputEditor->setBounds(formulaLiveDisplay->getBounds());
            formulaInputEditor->toFront(true);
        }
    }
    if (formulaLiveDisplay)
        formulaLiveDisplay->setVisible(! shouldEdit);
    if (editSaveButton)
        editSaveButton->setButtonText(shouldEdit ? TRANS("SaveButton") : TRANS("EditButton"));
    if (! shouldEdit)
        updateLiveFormulaView();
}

void NeuroCoreAudioProcessorEditor::updateLiveFormulaView()
{
    if (formulaLiveDisplay == nullptr)
        return;

    std::array<juce::String, Config::kNumUserParams> names;
    std::array<juce::Colour, Config::kNumUserParams> colours;
    std::array<float, Config::kNumUserParams> values {};
    for (int i = 0; i < Config::kNumUserParams; ++i)
    {
        names[(size_t) i]   = audioProcessor.getVariableName(i);
        colours[(size_t) i] = FormulaDisplayComponent::knobColour(i);
        if (auto* raw = audioProcessor.apvts.getRawParameterValue(EffectParameters::userParams[i]))
            values[(size_t) i] = raw->load();
        else
            values[(size_t) i] = 0.f;
    }
    formulaLiveDisplay->setVariableColours(names, colours);
    formulaLiveDisplay->setKnobValues(values);

    const auto script = formulaInputEditor ? formulaInputEditor->getText()
                                           : audioProcessor.getScript();
    formulaLiveDisplay->setFormula(script);
}

void NeuroCoreAudioProcessorEditor::timerCallback()
{
    if (! editing)
        updateLiveFormulaView();
    updateStatusBar();
    if (backdrop)
    {
        backdrop->setPeakFromDb (audioProcessor.getLoudnessDb());
        backdrop->advance (1.f / 24.f);
        const auto& s = cyberDirector.getState();
        lookAndFeel.animTime     = s.timeSec;
        lookAndFeel.glitchAmount = s.glitch;
        lookAndFeel.glitchSeed   = s.glitchSeed;
        lookAndFeel.peakPulse    = s.peakPulse;
        if (cyberDirector.needsAmbientRepaint())
            repaint();
    }

    if (mixSlider)
        mixSlider->tick (1.f / 30.f);

#if ! defined (NEUROCORE_SKIP_LICENSE_ENFORCEMENT)
    if (Config::kEnableLicensing && audioProcessor.isDemoMixLocked())
        if (auto* dry = audioProcessor.apvts.getParameter (EffectParameters::dryWet))
            if (dry->getValue() > 0.f)
                dry->setValueNotifyingHost (0.f);
#endif

    for (auto& pc : paramComponents)
        if (pc)
            pc->refreshValues();

    if (mixSlider && mixValue)
        mixValue->setText (juce::String (mixSlider->getValue() * 100.0, 0) + "%",
                           juce::dontSendNotification);

    // Combo readouts: append current selection into status so OS/Polisher always visible
    // (combo text itself already shows selection; status bar duplicates for HUD)
}

void NeuroCoreAudioProcessorEditor::updateStatusBar()
{
    if (statusBarLabel == nullptr)
        return;

    const double sr = audioProcessor.getSampleRate();
    const int srInt = sr > 0.0 ? (int) std::llround (sr) : 0;

    int osFactor = 1;
    if (auto* p = audioProcessor.apvts.getRawParameterValue (EffectParameters::oversampling))
    {
        // Combo indices: 0=1x, 1=2x, 2=4x, 3=8x
        static constexpr int factors[] = { 1, 2, 4, 8 };
        const int idx = juce::jlimit (0, 3, (int) std::lround (p->load()));
        osFactor = factors[idx];
    }

    float mixPct = 100.f;
    if (mixSlider)
        mixPct = (float) mixSlider->getValue() * 100.f;

    const bool lim = audioProcessor.isLimiterActive();
    const bool bypassed = bypassButton && bypassButton->getToggleState();
    const auto preset = audioProcessor.getCurrentPresetName();

    // Live mapped knob values A–D for the status HUD
    juce::String knobsHud;
    for (int i = 0; i < 4; ++i)
    {
        const char letter = static_cast<char> ('A' + i);
        float norm = 0.f;
        static constexpr const char* ids[4] = {
            EffectParameters::paramA, EffectParameters::paramB,
            EffectParameters::paramC, EffectParameters::paramD
        };
        if (auto* raw = audioProcessor.apvts.getRawParameterValue (ids[i]))
            norm = raw->load();
        // Prefer mapped display from paramInfo if present
        float shown = norm;
        const auto& info = audioProcessor.getParamInfo();
        for (const auto& pd : info)
        {
            if (pd.alias.length() == 1 && (pd.alias[0] - 'a') == i)
            {
                shown = pd.min + norm * (pd.max - pd.min);
                break;
            }
        }
        if (i) knobsHud << " ";
        knobsHud << letter << "=" << juce::String (shown, shown >= 100.f ? 0 : (shown >= 10.f ? 1 : 2));
    }

    juce::String polish = "None";
    if (polisherBox)
        polish = polisherBox->getText();

    const int latSm = audioProcessor.getLatencySamples();
    const float latMs = (srInt > 0) ? (1000.f * (float) latSm / (float) srInt) : 0.f;

    const bool cpuSafe = audioProcessor.isCpuProtectActive();
    const float cpuLoad = audioProcessor.getCpuLoad();
    juce::String s;
    s << (cpuSafe ? "SAFE" : (bypassed ? "BYPASS" : "LIVE"))
      << "  ·  CPU " << juce::String (cpuLoad * 100.f, 0) << "%"
      << "  ·  LAT " << latSm << "smp / " << juce::String (latMs, 2) << "ms"
      << "  ·  SR " << (srInt > 0 ? juce::String (srInt) : juce::String ("-"))
      << "  ·  OS " << osFactor << "x"
      << "  ·  MIX " << juce::String (mixPct, 0) << "%"
      << "  ·  " << knobsHud
      << "  ·  POL " << polish
      << "  ·  LIM " << (lim ? "ON" : "off");
    if (preset.isNotEmpty())
        s << "  ·  " << preset;
    if (Config::kEnableLicensing)
    {
        if (audioProcessor.isProductLicensed())
            s << "  ·  LIC " << audioProcessor.licensedEmail();
        else if (audioProcessor.isDemoMixLocked())
            s << "  ·  DEMO MIX 0";
        else
        {
            const int left = audioProcessor.demoSecondsRemaining();
            s << "  ·  DEMO " << juce::String (left / 60) << ":"
              << juce::String (left % 60).paddedLeft ('0', 2);
        }
    }

    statusBarLabel->setText (s, juce::dontSendNotification);
    // Limiter or bypass → hotter red cue (still meaningful, not decorative)
    statusBarLabel->setColour (juce::Label::textColourId,
                               (lim || bypassed || cpuSafe)
                                   ? NeuroCoreLookAndFeel::accent()
                                   : NeuroCoreLookAndFeel::accent().withAlpha (0.72f));

    if (errorLabel != nullptr)
    {
        if (cpuSafe)
            errorLabel->setText ("CPU overload — wet path paused. Retrying automatically. "
                                 "Lower oversampling if it keeps coming back.",
                                 juce::dontSendNotification);
        else if (errorLabel->getText().startsWith ("CPU overload"))
            errorLabel->setText ({}, juce::dontSendNotification);
    }
}

void NeuroCoreAudioProcessorEditor::refreshParameterControls()
{
    // Pull mapped ranges from the live DSL param lines (a=Drive [1.2, 7] etc.)
    const auto& paramInfo = audioProcessor.getParamInfo();
    std::array<float, Config::kNumUserParams> mapMin {};
    std::array<float, Config::kNumUserParams> mapMax {};
    std::array<bool, Config::kNumUserParams>  hasMap {};
    mapMin.fill (0.f);
    mapMax.fill (1.f);
    hasMap.fill (false);
    for (const auto& pd : paramInfo)
    {
        if (pd.alias.length() != 1)
            continue;
        const int idx = pd.alias[0] - 'a';
        if (idx < 0 || idx >= Config::kNumUserParams)
            continue;
        mapMin[(size_t) idx] = pd.min;
        mapMax[(size_t) idx] = pd.max;
        hasMap[(size_t) idx] = true;
    }

    for (int i = 0; i < paramComponents.size(); ++i)
    {
        const auto knobCol = FormulaDisplayComponent::knobColour(i);
        if (nameEditors[i])
        {
            nameEditors[i]->setText(audioProcessor.getVariableName(i), juce::dontSendNotification);
            nameEditors[i]->setColour(juce::TextEditor::textColourId, knobCol);
            nameEditors[i]->setColour(juce::TextEditor::outlineColourId, knobCol.withAlpha(0.65f));
        }
        if (paramComponents[i])
        {
            paramComponents[i]->setAliasName(audioProcessor.getVariableName(i));
            paramComponents[i]->setAccentColour(knobCol);
            if (hasMap[(size_t) i])
                paramComponents[i]->setMappedRange(mapMin[(size_t) i], mapMax[(size_t) i]);
            else
                paramComponents[i]->setMappedRange(0.f, 1.f);
        }
        bool active = audioProcessor.isParameterActive(i);
        if (paramComponents[i])
            paramComponents[i]->setEnabled(active);
        if (nameEditors[i])
        {
            // Alias editors stay readable; write only when linked
            nameEditors[i]->setEnabled (true);
            nameEditors[i]->setAlpha (active ? 1.0f : 0.55f);
            nameEditors[i]->setReadOnly (! active);
        }
    }
    updateLiveFormulaView();
}

void NeuroCoreAudioProcessorEditor::syncFromProcessor()
{
    // Always pull script after preset load (even if user was editing — preset wins)
    if (formulaInputEditor)
    {
        formulaInputEditor->setText(audioProcessor.getScript());
        if (editing)
            setFormulaEditMode(false);
    }

    refreshParameterControls();

    if (mixSlider && mixValue)
        mixValue->setText (juce::String (mixSlider->getValue() * 100.0, 0) + "%",
                           juce::dontSendNotification);

    if (currentPresetLabel)
    {
        const auto name = audioProcessor.getCurrentPresetName();
        // ASCII only: brand font Apex lacks bullet glyphs (showed as garbage "a")
        currentPresetLabel->setText (name.isNotEmpty() ? name : "Untitled",
                                     juce::dontSendNotification);
        currentPresetLabel->setColour (juce::Label::textColourId,
                                       name.isNotEmpty() ? NeuroCoreLookAndFeel::brightText()
                                                         : NeuroCoreLookAndFeel::mutedText());
        currentPresetLabel->setColour (juce::Label::backgroundColourId,
                                       NeuroCoreLookAndFeel::accent().withAlpha (
                                           name.isNotEmpty() ? 0.22f : 0.10f));
    }

    if (bypassButton && mixSlider)
    {
        const float mix = (float) mixSlider->getValue();
        const bool isBypassed = mix <= 0.001f;
        bypassButton->setToggleState(isBypassed, juce::dontSendNotification);
        bypassButton->setButtonText (isBypassed ? "BYPASSED" : "BYPASS");
        if (! isBypassed)
            mixBeforeBypass = mix;
    }

    if (errorLabel)
        errorLabel->setText({}, juce::dontSendNotification);

    updateStatusBar();
    repaint();
}

void NeuroCoreAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    syncFromProcessor();
}

void NeuroCoreAudioProcessorEditor::updateTranslations()
{
    if (mixLabel)
        mixLabel->setText ("MIX", juce::dontSendNotification);
    if (currentPresetLabel)
    {
        const auto name = audioProcessor.getCurrentPresetName();
        currentPresetLabel->setText (name.isNotEmpty() ? name : "Untitled",
                                     juce::dontSendNotification);
    }

    optimizeButton->setButtonText(TRANS("OptimizeButton"));
    if (copyFormulaButton)
        copyFormulaButton->setButtonText (TRANS ("CopyButton") == "CopyButton"
                                              ? "Copy" : TRANS ("CopyButton"));
    editSaveButton->setButtonText(editing ? TRANS("SaveButton") : TRANS("EditButton"));
    if (presetsButton)
        presetsButton->setButtonText(TRANS("Presets"));
    if (functionsButton)
        functionsButton->setButtonText(TRANS("Functions"));
    if (stagesButton)
        stagesButton->setButtonText(TRANS("StagesButton"));
    if (bypassButton)
        bypassButton->setButtonText (bypassButton->getToggleState() ? "BYPASSED" : "BYPASS");
    if (editorFontLabel)
        editorFontLabel->setText ("Text", juce::dontSendNotification);
    if (polisherLabel)
        polisherLabel->setText(TRANS("PolisherLabel"), juce::dontSendNotification);
    if (helpButton)
        helpButton->setButtonText(TRANS("HelpButton"));
    if (licenseButton)
        licenseButton->setButtonText ("License");
    if (oversamplingLabel)
        oversamplingLabel->setText(TRANS("OversamplingLabel"), juce::dontSendNotification);
}

void NeuroCoreAudioProcessorEditor::paintHudChrome (juce::Graphics& g)
{
    // Frame panels with HUD corners ONLY — never fill opaque over OpenGL children
    struct PanelTag { const juce::Component* c; const char* tag; bool fill; };
    const PanelTag panels[] = {
        { formulaLiveDisplay.get(),  "DSP CORE", true },
        { formulaInputEditor.get(),  "EDIT", true },
        { loudnessMeter.get(),       "LOUDNESS", false },
        { inputDisplay.get(),        "IN // PRE", false },
        { outputDisplay.get(),       "OUT // POST", false },
    };

    for (const auto& p : panels)
    {
        if (p.c == nullptr || ! p.c->isVisible() || p.c->getWidth() < 4)
            continue;
        auto r = p.c->getBounds().toFloat().expanded (2.f);
        if (p.fill)
        {
            g.setColour (juce::Colour (0xcc050505));
            g.fillRect (r);
        }
        // Animated corner pulse
        const float pulse = 0.45f + 0.55f * std::sin (lookAndFeel.animTime * 5.f
                                                       + r.getX() * 0.01f);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.35f + 0.45f * pulse));
        NeuroCoreLookAndFeel::drawHudFrame (g, r, p.tag);

        // Live LED on frame corner
        const bool hot = p.c == outputDisplay.get()
                             ? audioProcessor.isLimiterActive()
                             : (lookAndFeel.peakPulse > 0.15f);
        g.setColour (hot ? NeuroCoreLookAndFeel::accent()
                         : NeuroCoreLookAndFeel::accent().withAlpha (0.25f));
        g.fillEllipse (r.getRight() - 10.f, r.getY() + 4.f, 5.f, 5.f);
    }

    // Bottom threat / link strip
    g.setColour (juce::Colour (0xff080000));
    g.fillRect (0, getHeight() - 3, getWidth(), 3);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.5f + 0.5f * lookAndFeel.peakPulse));
    const int barW = (int) (getWidth() * juce::jlimit (0.05f, 1.f, lookAndFeel.peakPulse));
    g.fillRect (0, getHeight() - 3, barW, 3);
}

void NeuroCoreAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (backdrop != nullptr)
    {
        backdrop->setBounds (getLocalBounds());
        backdrop->paint (g);
    }
    else
    {
        g.fillAll (juce::Colours::black);
    }

    // Never layout here. paint() used to call performLayout(getLocalBounds()),
    // which parked the lockup on top of the HUD and covered the OS banner.
    if (assemblingWindows)
        applyWindowAssemble();

    paintHudChrome (g);
}

void NeuroCoreAudioProcessorEditor::visibilityChanged()
{
    cyberDirector.setVisible (isShowing());
    if (isShowing())
        startWindowAssemble();
}

void NeuroCoreAudioProcessorEditor::applyOverlayMotion (ModalOverlay& overlay)
{
    overlay.setMotion (cyberDirector.getState().motion);
}

void NeuroCoreAudioProcessorEditor::dismissOverlayNow (std::unique_ptr<ModalOverlay>& overlay)
{
    if (overlay == nullptr)
        return;
    overlay->skipToEnd();
    overlay.reset();
    syncGlCover();
}

void NeuroCoreAudioProcessorEditor::syncGlCover()
{
    const bool cover =
        (presetOverlay != nullptr && presetOverlay->isShowing())
        || (functionsOverlay != nullptr && functionsOverlay->isShowing())
        || (stagesOverlay != nullptr && stagesOverlay->isShowing())
        || (validationOverlay != nullptr && validationOverlay->isShowing());
    if (loudnessMeter != nullptr)
        loudnessMeter->setCoveredByOverlay (cover);
}

void NeuroCoreAudioProcessorEditor::startWindowAssemble()
{
    if (assemblePlayed || assemblingWindows)
        return;

    assemblePlayed = true;
    assemblingWindows = true;
    assembleElapsed = 0.f;
    assembleStamp = 0.0;
    if (layoutRoot)
        ui::performLayout (*layoutRoot, chromeBounds());
    captureAssembleTargets();
    if (assembleSlots.empty())
    {
        assemblingWindows = false;
        assemblePlayed = false;
        return;
    }
    applyWindowAssemble();
    assembleVblank = juce::VBlankAttachment { this, [this] (double now) { onAssembleVBlank (now); } };
}

void NeuroCoreAudioProcessorEditor::captureAssembleTargets()
{
    assembleSlots.clear();
    auto add = [this] (juce::Component* c, float delay)
    {
        if (c == nullptr || ! c->isVisible() || c->getWidth() < 8 || c->getHeight() < 8)
            return;
        assembleSlots.push_back ({ c, c->getBounds(), randomClipReveal (assembleRng), delay });
    };

    add (formulaLiveDisplay.get(), 0.00f);
    add (loudnessMeter.get(),      0.04f);
    add (inputDisplay.get(),       0.07f);
    add (outputDisplay.get(),      0.10f);
    for (size_t i = 0; i < paramComponents.size(); ++i)
        add (paramComponents[i].get(), 0.02f + 0.025f * (float) i);
}

void NeuroCoreAudioProcessorEditor::applyWindowAssemble()
{
    for (auto& slot : assembleSlots)
    {
        if (slot.c == nullptr)
            continue;
        const float local = (assembleElapsed - slot.delaySec) / kClipRevealSec;
        if (local <= 0.f)
        {
            slot.c->setVisible (false);
            continue;
        }
        slot.c->setVisible (true);
        slot.c->setBounds (revealBounds (slot.target, slot.type, juce::jlimit (0.f, 1.f, local)));
    }
    syncGlCover();
}

void NeuroCoreAudioProcessorEditor::onAssembleVBlank (double nowSec)
{
    if (! assemblingWindows)
        return;
    if (assembleStamp <= 0.0)
        assembleStamp = nowSec;
    const float dt = juce::jlimit (0.f, 0.05f, (float) (nowSec - assembleStamp));
    assembleStamp = nowSec;
    assembleElapsed += dt;
    applyWindowAssemble();
    repaint();

    float last = 0.f;
    for (const auto& slot : assembleSlots)
        last = juce::jmax (last, slot.delaySec);
    if (assembleElapsed >= last + kClipRevealSec)
    {
        assemblingWindows = false;
        assembleVblank = {};
        if (layoutRoot)
            ui::performLayout (*layoutRoot, chromeBounds());
        if (formulaInputEditor && formulaLiveDisplay)
            formulaInputEditor->setBounds (formulaLiveDisplay->getBounds());
        setFormulaEditMode (editing);
        syncGlCover();
        repaint();
    }
}

juce::Rectangle<int> NeuroCoreAudioProcessorEditor::chromeBounds() const
{
    auto r = getLocalBounds();
    r.removeFromTop (Config::kHudHeaderHeight);
    return r;
}

void NeuroCoreAudioProcessorEditor::resized()
{
    if (backdrop != nullptr)
        backdrop->setBounds (getLocalBounds());

    if (layoutRoot)
        ui::performLayout (*layoutRoot, chromeBounds());
    if (assemblingWindows)
    {
        captureAssembleTargets();
        applyWindowAssemble();
    }
    else if (isShowing() && ! assemblePlayed)
    {
        startWindowAssemble();
    }

    // Keep the code editor stacked on the live formula panel when editing
    if (formulaInputEditor && formulaLiveDisplay)
        formulaInputEditor->setBounds(formulaLiveDisplay->getBounds());

    if (statusBarLabel)
    {
        statusBarLabel->setFont (NeuroCoreLookAndFeel::monoFont (11.f));
        statusBarLabel->setColour (juce::Label::textColourId,
                                   NeuroCoreLookAndFeel::accent().withAlpha (0.72f));
    }
    if (audioSettingsButton)
    {
        audioSettingsButton->setColour (juce::TextButton::textColourOffId,
                                        NeuroCoreLookAndFeel::accent());
    }
    if (mixLabel)
    {
        mixLabel->setJustificationType (juce::Justification::centredLeft);
        mixLabel->setFont (NeuroCoreLookAndFeel::monoFont (12.f));
        mixLabel->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    }
    for (auto* lbl : { oversamplingLabel.get(), polisherLabel.get() })
    {
        if (lbl != nullptr)
        {
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(NeuroCoreLookAndFeel::brandFont(11.f, true));
            lbl->setColour(juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
        }
    }
    if (mixValue)
    {
        mixValue->setJustificationType (juce::Justification::centredRight);
        mixValue->setFont (NeuroCoreLookAndFeel::monoFont (13.f));
        mixValue->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    }
    if (currentPresetLabel)
    {
        currentPresetLabel->setFont (NeuroCoreLookAndFeel::brandFont (14.f, true));
        currentPresetLabel->setColour (juce::Label::backgroundColourId,
                                       NeuroCoreLookAndFeel::accent().withAlpha (0.18f));
        currentPresetLabel->setJustificationType (juce::Justification::centredLeft);
    }
    if (errorLabel)
    {
        errorLabel->setJustificationType(juce::Justification::centredLeft);
        errorLabel->setFont (NeuroCoreLookAndFeel::monoFont (12.f));
        errorLabel->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    }
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
    dismissOverlayNow (presetOverlay);
    auto content = std::make_unique<PresetContentComponent>(audioProcessor, lookAndFeel);
    auto* ptr = content.get();

    presetOverlay = std::make_unique<ModalOverlay>();
    presetOverlay->setMode(OverlayMode::Closable);
    presetOverlay->setTitle ("Preset Explorer");
    presetOverlay->setPreferredContentSize (juce::jmin (getWidth() - 24, 1100),
                                            juce::jmin (getHeight() - 24, 700));
    presetOverlay->setContent(std::move(content));
    applyOverlayMotion (*presetOverlay);
    presetOverlay->show(*this);
    syncGlCover();
    presetOverlay->onClose = [this] { presetOverlay.reset(); syncGlCover(); };

    auto refreshAfterPreset = [this]
    {
        setFormulaEditMode (false);
        if (formulaInputEditor)
            formulaInputEditor->setText (audioProcessor.getScript());
        updateLiveFormulaView();
        syncFromProcessor();

        const auto q = audioProcessor.analyseFormulaQuality (audioProcessor.getScript());
        if (errorLabel)
        {
            errorLabel->setColour (juce::Label::textColourId,
                                   q.ok ? (q.warnings.isEmpty() ? juce::Colour (0xff7dcea0)
                                                                : juce::Colour (0xfff4d03f))
                                        : juce::Colour (0xffff6b6b));
            errorLabel->setText (q.summary()
                                     + (q.errors.isEmpty()
                                            ? juce::String()
                                            : ("  ·  " + q.errors[0])),
                                 juce::dontSendNotification);
        }
    };

    ptr->onLoaded = [this, refreshAfterPreset]
    {
        if (presetOverlay != nullptr)
            presetOverlay->requestClose();
        refreshAfterPreset();
    };
    ptr->onSaved = [this] { syncFromProcessor(); };
    ptr->onClose = [this] { presetOverlay.reset(); syncGlCover(); };
}

void NeuroCoreAudioProcessorEditor::showFunctionsOverlay()
{
    dismissOverlayNow (functionsOverlay);
    auto content = std::make_unique<FunctionsContentComponent>(audioProcessor);
    auto* ptr = content.get();

    functionsOverlay = std::make_unique<ModalOverlay>();
    functionsOverlay->setMode(OverlayMode::Closable);
    functionsOverlay->setTitle(TRANS("Functions"));
    functionsOverlay->setPreferredContentSize (juce::jmin (getWidth() - 16, 1080),
                                               juce::jmin (getHeight() - 16, 740));
    functionsOverlay->setContent(std::move(content));
    applyOverlayMotion (*functionsOverlay);
    functionsOverlay->show(*this);
    syncGlCover();

    ptr->onInsert = [this](const juce::String& text)
    {
        if (formulaInputEditor)
            formulaInputEditor->insertTextAtCaret(text);
    };
    ptr->onClose = [this]{ functionsOverlay.reset(); syncGlCover(); };
    functionsOverlay->onClose = [this]{ functionsOverlay.reset(); syncGlCover(); };
}

void NeuroCoreAudioProcessorEditor::hideFunctionsOverlay()
{
    if (functionsOverlay)
        functionsOverlay->requestClose();
}

void NeuroCoreAudioProcessorEditor::showStagesOverlay()
{
    dismissOverlayNow (stagesOverlay);
    auto content = std::make_unique<StagesContentComponent>(audioProcessor);
    auto* ptr = content.get();

    stagesOverlay = std::make_unique<ModalOverlay>();
    stagesOverlay->setMode(OverlayMode::Closable);
    stagesOverlay->setTitle(TRANS("StagesTitle"));
    stagesOverlay->setPreferredContentSize (juce::jmin (getWidth() - 40, 880),
                                            juce::jmin (getHeight() - 40, 520));
    stagesOverlay->setContent(std::move(content));
    applyOverlayMotion (*stagesOverlay);
    stagesOverlay->show(*this);
    syncGlCover();

    ptr->onClose = [this] { stagesOverlay.reset(); syncGlCover(); };
    stagesOverlay->onClose = [this] { stagesOverlay.reset(); syncGlCover(); };
}

void NeuroCoreAudioProcessorEditor::hideStagesOverlay()
{
    if (stagesOverlay)
        stagesOverlay->requestClose();
}
void NeuroCoreAudioProcessorEditor::hidePresetOverlay()
{
    if (presetOverlay)
        presetOverlay->requestClose();
}

void NeuroCoreAudioProcessorEditor::validateAndOverlay(const juce::String& expr)
{
    dismissOverlayNow (validationOverlay);

    auto content = std::make_unique<ValidationContentComponent>(audioProcessor, expr);
    auto* ptr = content.get();

    validationOverlay = std::make_unique<ModalOverlay>();
    validationOverlay->setMode(OverlayMode::Blocking);
    validationOverlay->setTitle("Validating Script...");
    validationOverlay->setContent(std::move(content));
    applyOverlayMotion (*validationOverlay);
    validationOverlay->onClose = [this] { validationOverlay.reset(); syncGlCover(); };
    validationOverlay->show(*this);
    syncGlCover();

    ptr->onProgress = [this] (const juce::String& line)
    {
        if (validationOverlay != nullptr)
            validationOverlay->setLiveStatus (line);
    };

    ptr->onResult = [this, expr](bool stable)
    {
        if (validationOverlay != nullptr)
            validationOverlay->requestClose();

        // Always compute quality metric for the editor status line
        const auto q = audioProcessor.analyseFormulaQuality (expr);
        const auto qualityHint = q.summary()
            + (q.warnings.isEmpty() ? juce::String()
                                    : ("  |  " + q.warnings[0]));

        juce::String err;
        if (stable && q.ok && audioProcessor.setFormula (expr, err))
        {
            setFormulaEditMode (false);
            errorLabel->setColour (juce::Label::textColourId,
                                   q.warnings.isEmpty() ? juce::Colour (0xff7dcea0)
                                                        : juce::Colour (0xfff4d03f));
            errorLabel->setText (qualityHint, juce::dontSendNotification);
            refreshParameterControls();
        }
        else if (stable && ! q.ok)
        {
            // Stability OK but quality gate failed — still refuse apply
            errorLabel->setColour (juce::Label::textColourId, juce::Colour (0xffff6b6b));
            errorLabel->setText (qualityHint + "  -  "
                                 + q.errors.joinIntoString ("; "),
                                 juce::dontSendNotification);
        }
        else
        {
            errorLabel->setColour (juce::Label::textColourId, juce::Colour (0xffff6b6b));
            if (err.isNotEmpty())
                errorLabel->setText (err, juce::dontSendNotification);
            else
                errorLabel->setText (qualityHint + "  -  validation failed",
                                     juce::dontSendNotification);
        }
    };
}





