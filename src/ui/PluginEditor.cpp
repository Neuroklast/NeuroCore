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
#include "LicenseInfoComponent.h"
#include "SettingsContentComponent.h"
#include "StandaloneAudioSettings.h"
#include "../utils/FormulaQuality.h"
#include "fx/CyberFxTypes.h"
#include "fx/CyberClip.h"
#include "fx/CyberChrome.h"
#include "GraphCanvasComponent.h"
#include "../utils/UiSettings.h"


//==============================================================================
NeuroKoreAudioProcessorEditor::NeuroKoreAudioProcessorEditor (NeuroKoreAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    Localiser::getInstance().addListener(this);
    audioProcessor.addChangeListener(this);

    setResizable (true, true);
    setResizeLimits (Config::kUiMinWindowWidth, Config::kUiMinWindowHeight,
                     Config::kUiMaxWindowWidth, Config::kUiMaxWindowHeight);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (Config::kUiAspectRatio);
    setOpaque(true);

    scaledRoot = std::make_unique<juce::Component>();
    scaledRoot->setOpaque (false);
    addAndMakeVisible (*scaledRoot);

    cyberDirector.setMotion (UiSettings::get().motion());

    backdrop = std::make_unique<CyberBackdropComponent> (cyberDirector);

    brandLockup = std::make_unique<BrandLockup> (
        lookAndFeel.getNkLogo(),
        juce::String (Config::kProductName),
        juce::String (Config::kBrandByline) + "  v" + PLUGIN_VERSION);
    logoGlitch.director = &cyberDirector;
    brandLockup->addMouseListener (&logoGlitch, false);
    scaledRoot->addAndMakeVisible (*brandLockup);

    statusBarLabel = std::make_unique<juce::Label>();
    statusBarLabel->setJustificationType(juce::Justification::centredLeft);
    statusBarLabel->setColour(juce::Label::textColourId, NeuroKoreLookAndFeel::inkMuted());
    statusBarLabel->setFont(NeuroKoreLookAndFeel::monoFont(11.f));
    statusBarLabel->setMinimumHorizontalScale(0.55f);
    scaledRoot->addAndMakeVisible(*statusBarLabel);

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
            body = "NeuroKore Help\n\nThe manual could not be loaded. Try closing Help and opening it again.";

        auto viewer = std::make_unique<HelpContentComponent> (body);

        validationOverlay = std::make_unique<ModalOverlay>();
        validationOverlay->setMode (OverlayMode::Closable);
        validationOverlay->setTitle ("Help / User Manual");
        validationOverlay->setPreferredContentSize (0, 0);
        validationOverlay->setContent (std::move (viewer));
        applyOverlayMotion (*validationOverlay);
        validationOverlay->show (*scaledRoot);
        syncGlCover();
        validationOverlay->onClose = [this] { validationOverlay.reset(); syncGlCover(); };
    };
    scaledRoot->addChildComponent(*helpButton);

    licenseButton = std::make_unique<juce::TextButton> ("License");
    licenseButton->onClick = [this]
    {
        if (audioProcessor.isProductLicensed())
            showLicenseInfoOverlay();
        else
            promptImportLicense();
    };
    scaledRoot->addChildComponent (*licenseButton);
    refreshLicenseButton();

    editorFontLabel = std::make_unique<juce::Label> ("", "Text");
    editorFontLabel->setVisible (false);
    scaledRoot->addChildComponent (*editorFontLabel);

    editorFontMinusButton = std::make_unique<juce::TextButton> ("Aa-");
    editorFontPlusButton  = std::make_unique<juce::TextButton> ("Aa+");
    editorFontSizeLabel   = std::make_unique<juce::Label> ("", juce::String ((int) Config::kDefaultEditorFontPt));
    editorFontSizeLabel->setVisible (false);
    editorFontMinusButton->setTooltip ("Smaller formula text");
    editorFontPlusButton->setTooltip ("Larger formula text");
    editorFontMinusButton->onClick = [this]
    {
        applyEditorFontSize (editorFontHeight - Config::kEditorFontStepPt);
    };
    editorFontPlusButton->onClick = [this]
    {
        applyEditorFontSize (editorFontHeight + Config::kEditorFontStepPt);
    };
    scaledRoot->addAndMakeVisible (*editorFontMinusButton);
    scaledRoot->addAndMakeVisible (*editorFontPlusButton);
    scaledRoot->addAndMakeVisible (*editorFontSizeLabel);

    presetsButton = std::make_unique<juce::TextButton>(TRANS("Presets"));
    presetsButton->onClick = [this] { showPresetOverlay(); };
    scaledRoot->addAndMakeVisible(*presetsButton);

    // Cyber OS bypass — TextButton with toggle state, painted by LookAndFeel angular style
    bypassButton = std::make_unique<juce::TextButton>("BYPASS");
    bypassButton->setClickingTogglesState (true);
    bypassButton->setColour (juce::TextButton::buttonColourId, NeuroKoreLookAndFeel::surfaceHigh());
    bypassButton->setColour (juce::TextButton::buttonOnColourId, NeuroKoreLookAndFeel::accent());
    bypassButton->setColour (juce::TextButton::textColourOffId, NeuroKoreLookAndFeel::ink());
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
        applyBypassMixLock();
    };
    scaledRoot->addAndMakeVisible(*bypassButton);

    functionsButton = std::make_unique<juce::TextButton>(TRANS("Functions"));
    functionsButton->onClick = [this] { showFunctionsOverlay(); };
    scaledRoot->addAndMakeVisible(*functionsButton);

    stagesButton = std::make_unique<juce::TextButton>("Stages");
    stagesButton->onClick = [this] { showStagesOverlay(); };
    scaledRoot->addAndMakeVisible(*stagesButton);

    settingsButton = std::make_unique<juce::TextButton> ("Settings");
    settingsButton->setTooltip ("Animation, Live/Studio, scale, formula text, and standalone audio.");
    settingsButton->onClick = [this] { showSettingsOverlay(); };
    scaledRoot->addAndMakeVisible (*settingsButton);

    liveButton = std::make_unique<juce::TextButton> ("LIVE");
    liveButton->setClickingTogglesState (true);
    liveButton->setToggleState (UiSettings::get().liveMode(), juce::dontSendNotification);
    liveButton->setTooltip ("Live: min-phase oversampling, near-zero latency. Off = Studio (linear-phase).");
    liveButton->onClick = [this]
    {
        audioProcessor.setLiveMode (liveButton->getToggleState());
        liveButton->setButtonText (liveButton->getToggleState() ? "LIVE" : "STUDIO");
        updateStatusBar();
    };
    liveButton->setButtonText (UiSettings::get().liveMode() ? "LIVE" : "STUDIO");
    scaledRoot->addAndMakeVisible (*liveButton);

    workspaceGraphButton = std::make_unique<juce::TextButton> ("Graph");
    workspaceScriptButton = std::make_unique<juce::TextButton> ("Script");
    workspaceGraphButton->setClickingTogglesState (true);
    workspaceScriptButton->setClickingTogglesState (true);
    workspaceGraphButton->setRadioGroupId (0x47525048);
    workspaceScriptButton->setRadioGroupId (0x47525048);
    workspaceGraphButton->setTooltip ("Node patcher. Drag a module to move it. Drag a port to another port to patch.");
    workspaceScriptButton->setTooltip ("Formula text. Always the source of truth.");
    workspaceGraphButton->onClick = [this] { setWorkspaceMode (WorkspaceGraph); };
    workspaceScriptButton->onClick = [this] { setWorkspaceMode (WorkspaceScript); };
    scaledRoot->addAndMakeVisible (*workspaceGraphButton);
    scaledRoot->addAndMakeVisible (*workspaceScriptButton);

    graphCanvas = std::make_unique<GraphCanvasComponent> (audioProcessor);
    graphCanvas->onOpenIr = [this] (juce::String slot) { showIrOverlay (slot); };
    graphCanvas->onScriptChanged = [this]
    {
        if (formulaInputEditor)
            formulaInputEditor->setText (audioProcessor.getScript());
        updateLiveFormulaView();
    };
    graphCanvas->knobForIndex = [this] (int i) -> juce::Component*
    {
        if (i < 0 || i >= (int) paramComponents.size())
            return nullptr;
        return paramComponents[(size_t) i].get();
    };
    graphCanvas->setScript (audioProcessor.getScript());
    scaledRoot->addAndMakeVisible (*graphCanvas);

    // Language fixed to English (UI switch removed — brand default)


    const float startAngle = juce::MathConstants<float>::pi * 4.0f / 3.0f;
    const float endAngle   = juce::MathConstants<float>::pi * 8.0f / 3.0f;

    for (int i = 0; i < (int)paramComponents.size(); ++i)
    {
        // ID a,b,c,d
        auto paramId = juce::String::charToString(static_cast<juce_wchar>('a' + i));
        const auto knobCol = NeuroKoreLookAndFeel::accent();

        // 2.1) ParameterComponent bauen und sichtbar machen
        paramComponents[i] = std::make_unique<ui::ParameterComponent>(
            audioProcessor.apvts,
            paramId,
            audioProcessor.getVariableName(i));
        paramComponents[i]->setMidiLearnManager(&audioProcessor.midiLearnManager);
        paramComponents[i]->setAccentColour(knobCol);
        paramComponents[i]->onAliasChanged = [this, i] (juce::String name)
        {
            const auto old = audioProcessor.getVariableName (i);
            applyKnobDisplayName (i, name);
            audioProcessor.recordNameChange (i, old, name);
            writeParamNameToScript (i, name);
            updateLiveFormulaView();
        };
        scaledRoot->addAndMakeVisible(*paramComponents[i]);

        // 2.2) TextEditor für Alias-Name — outline matches knob colour
        nameEditors[i] = std::make_unique<juce::TextEditor>();
        nameEditors[i]->setText(audioProcessor.getVariableName(i),
            juce::dontSendNotification);
        nameEditors[i]->onTextChange = [this, i]
            {
                auto newName = nameEditors[i]->getText();
                applyKnobDisplayName (i, newName);
                writeParamNameToScript (i, newName);
                updateLiveFormulaView();
            };
        nameEditors[i]->setJustification(juce::Justification::centred);
        nameEditors[i]->setFont(juce::Font(12.f));
        nameEditors[i]->setColour(juce::TextEditor::backgroundColourId, NeuroKoreLookAndFeel::surfaceHigh());
        nameEditors[i]->setColour(juce::TextEditor::textColourId, knobCol);
        nameEditors[i]->setColour(juce::TextEditor::outlineColourId, knobCol.withAlpha(0.65f));
        nameEditors[i]->setColour(juce::TextEditor::focusedOutlineColourId, knobCol);
        nameEditors[i]->setVisible (false);
        scaledRoot->addChildComponent(*nameEditors[i]);
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
        if (bypassButton && bypassButton->getToggleState())
        {
            if (mixSlider->getValue() > 0.001)
            {
                mixSlider->setValue (0.0, juce::dontSendNotification);
                if (auto* p = audioProcessor.apvts.getParameter (EffectParameters::dryWet))
                    p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (0.0f));
            }
        }
        if (mixValue)
            mixValue->setText (juce::String (mixSlider->getValue() * 100.0f, 0) + "%",
                               juce::dontSendNotification);
    };
    scaledRoot->addAndMakeVisible (*mixSlider);

    if (auto* p = audioProcessor.apvts.getParameter (EffectParameters::outputGain))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (1.0f));

    mixLabel = std::make_unique<juce::Label> ("", "MIX");
    mixLabel->setMinimumHorizontalScale (1.0f);
    scaledRoot->addAndMakeVisible (*mixLabel);

    currentPresetLabel = std::make_unique<juce::Label>("currentPreset", juce::String());
    currentPresetLabel->setJustificationType (juce::Justification::centred);
    currentPresetLabel->setMinimumHorizontalScale (1.0f);
    currentPresetLabel->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::brightText());
    currentPresetLabel->setColour (juce::Label::backgroundColourId,
                                   NeuroKoreLookAndFeel::accent().withAlpha (0.18f));
    currentPresetLabel->setColour (juce::Label::outlineColourId, NeuroKoreLookAndFeel::accent());
    currentPresetLabel->setFont (NeuroKoreLookAndFeel::brandFont (Config::kPresetChipFontPt, true));
    currentPresetLabel->setBorderSize ({ 2, 8, 2, 8 });
    currentPresetLabel->setTooltip ("Current preset — arrows step through the library");
    scaledRoot->addAndMakeVisible (*currentPresetLabel);

    auto stylePresetArrow = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,
                     NeuroKoreLookAndFeel::accent().withAlpha (0.16f));
        b.setColour (juce::TextButton::buttonOnColourId,
                     NeuroKoreLookAndFeel::accent().withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, NeuroKoreLookAndFeel::brightText());
        b.setColour (juce::TextButton::textColourOnId, NeuroKoreLookAndFeel::brightText());
    };
    presetPrevButton = std::make_unique<juce::TextButton> ("<");
    presetPrevButton->setName ("presetPrev");
    presetPrevButton->setTooltip ("Previous preset");
    presetPrevButton->onClick = [this] { audioProcessor.stepPreset (-1); };
    stylePresetArrow (*presetPrevButton);
    scaledRoot->addAndMakeVisible (*presetPrevButton);

    presetNextButton = std::make_unique<juce::TextButton> (">");
    presetNextButton->setName ("presetNext");
    presetNextButton->setTooltip ("Next preset");
    presetNextButton->onClick = [this] { audioProcessor.stepPreset (1); };
    stylePresetArrow (*presetNextButton);
    scaledRoot->addAndMakeVisible (*presetNextButton);

    polisherLabel = std::make_unique<juce::Label>("", TRANS("PolisherLabel"));
    polisherLabel->setMinimumHorizontalScale(1.0f);
    scaledRoot->addAndMakeVisible (*polisherLabel);

    polisherBox = std::make_unique<juce::ComboBox>();
    polisherBox->addItemList (juce::StringArray { "None", "Hard Clip", "Limiter" }, 1);
    polisherAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, EffectParameters::polisherMode, *polisherBox);
    scaledRoot->addAndMakeVisible (*polisherBox);

    oversamplingLabel = std::make_unique<juce::Label>("", TRANS("OversamplingLabel"));
    oversamplingLabel->setMinimumHorizontalScale(1.0f);
    scaledRoot->addAndMakeVisible(*oversamplingLabel);

    oversamplingBox = std::make_unique<juce::ComboBox>();
    oversamplingBox->addItemList(juce::StringArray { "1x", "2x", "4x", "8x" }, 1);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, EffectParameters::oversampling, *oversamplingBox);
    scaledRoot->addAndMakeVisible(*oversamplingBox);


    mixValue = std::make_unique<juce::Label>();
    mixValue->setJustificationType (juce::Justification::centred);
    mixValue->setMinimumHorizontalScale (1.0f);
    scaledRoot->addAndMakeVisible (*mixValue);

    inputChannelSwitch = std::make_unique<InputChannelSwitch> (audioProcessor.apvts);
    scaledRoot->addAndMakeVisible (*inputChannelSwitch);

    formulaInputEditor = std::make_unique<DslTerminalEditor>(audioProcessor);
    formulaInputEditor->onOpenIrSlot = [this] (juce::String slot) { showIrOverlay (slot); };
    formulaInputEditor->irCaptionForSlot = [this] (juce::String slot)
    {
        const auto n = audioProcessor.getIrName (slot);
        return n.isNotEmpty() ? n : juce::String ("drop / change / clear");
    };
    formulaInputEditor->onScriptTextChanged = [this]
    {
        if (ignoreScriptNameSync || workspaceMode != WorkspaceScript)
            return;
        if (formulaInputEditor != nullptr)
            syncParamNamesFromScript (formulaInputEditor->getText());
    };
    formulaInputEditor->setText(audioProcessor.getScript());
    formulaInputEditor->setOpaque(true);
    formulaInputEditor->setReadOnly(true);
    formulaInputEditor->setVisible(false);
    scaledRoot->addChildComponent(*formulaInputEditor);

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
        formulaLiveDisplay->onOpenIrSlot = [this] (juce::String slot) { showIrOverlay (slot); };
        formulaLiveDisplay->irCaptionForSlot = [this] (juce::String slot)
        {
            const auto n = audioProcessor.getIrName (slot);
            return n.isNotEmpty() ? n : juce::String ("drop / change / clear");
        };
        formulaLiveDisplay->setFormula(audioProcessor.getScript());
    }
    scaledRoot->addAndMakeVisible(*formulaLiveDisplay);
    applyEditorFontSize (UiSettings::get().editorFontPt());
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
        validationOverlay->setPreferredContentSize (0, 0);
        validationOverlay->setContent (std::move (content));
        applyOverlayMotion (*validationOverlay);
        validationOverlay->show (*scaledRoot);
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
    scaledRoot->addAndMakeVisible(*optimizeButton);

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
    scaledRoot->addAndMakeVisible (*copyFormulaButton);

    editSaveButton = std::make_unique<juce::TextButton>(TRANS("EditButton"));
    editSaveButton->setTooltip ("Edit the formula text. Save applies it.");
    editSaveButton->onClick = [this]
    {
        if (! editing)
        {
            setWorkspaceMode (WorkspaceScript);
            setFormulaEditMode (true);
        }
        else
            validateAndOverlay(formulaInputEditor->getText());
    };
    scaledRoot->addAndMakeVisible(*editSaveButton);

    // Timer started after layout (cyber OS + live formula)

    errorLabel = std::make_unique<juce::Label>();
    errorLabel->setMinimumHorizontalScale(1.0f);
    errorLabel->setColour(juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
    errorLabel->setColour(juce::Label::backgroundColourId, NeuroKoreLookAndFeel::canvas());
    errorLabel->setFont (NeuroKoreLookAndFeel::monoFont (12.f));
    scaledRoot->addAndMakeVisible(*errorLabel);

    inputDisplay  = std::make_unique<ScopeDeck>(audioProcessor, WaveformDisplayComponent::Type::Input);
    outputDisplay = std::make_unique<ScopeDeck>(audioProcessor, WaveformDisplayComponent::Type::Output);
    inputDisplay->setExtrasOpen (true);
    outputDisplay->setExtrasOpen (true);
    scaledRoot->addAndMakeVisible(*inputDisplay);
    scaledRoot->addAndMakeVisible(*outputDisplay);

    loudnessMeter = std::make_unique<LoudnessMeterComponent>(audioProcessor);
    loudnessMeter->setVisible (false);
    scaledRoot->addChildComponent(*loudnessMeter);

    using namespace ui;
    const int pad = Config::kUiPadding;

    layoutRoot = makeColumn();
    layoutRoot->margin = pad;
    layoutRoot->innerMargin = 4;
    layoutRoot->drawBorder = false;

    auto addChrome = [&] (ui::LayoutNode& parent, juce::Component* c, float w)
    {
        auto n = makeLeaf (c, w);
        n->maxHeight = Config::kChromeControlHeight;
        parent.addChild (std::move (n));
    };

    auto toolbar = makeRow (0.0f);
    toolbar->innerMargin = 4;
    toolbar->minHeight = Config::kToolbarRowMinHeight;
    toolbar->maxHeight = Config::kToolbarRowMaxHeight;
    toolbar->addChild (makeLeaf (brandLockup.get(), 2.8f));
    toolbar->addChild (makeLeaf (presetsButton.get(), 0.82f));
    auto presetNav = makeRow (2.2f);
    presetNav->innerMargin = 3;
    presetNav->addChild (makeLeaf (presetPrevButton.get(), 0.32f));
    presetNav->addChild (makeLeaf (currentPresetLabel.get(), 2.1f));
    presetNav->addChild (makeLeaf (presetNextButton.get(), 0.32f));
    toolbar->addChild (std::move (presetNav));
    toolbar->addChild (makeLeaf (functionsButton.get(), 0.82f));
    toolbar->addChild (makeLeaf (stagesButton.get(), 0.82f));
    toolbar->addChild (makeLeaf (settingsButton.get(), 0.82f));
    toolbar->addChild (makeLeaf (liveButton.get(), 0.82f));
    toolbar->addChild (makeLeaf (bypassButton.get(), 0.82f));
    layoutRoot->addChild (std::move (toolbar));

    auto tools = makeRow (0.0f);
    tools->innerMargin = 4;
    tools->minHeight = Config::kToolsRowHeight;
    tools->maxHeight = Config::kToolsRowHeight;
    tools->addChild (makeLeaf (oversamplingLabel.get(), 0.7f));
    addChrome (*tools, oversamplingBox.get(), 0.55f);
    tools->addChild (makeLeaf (polisherLabel.get(), 0.55f));
    addChrome (*tools, polisherBox.get(), 0.85f);
    tools->addChild (makeLeaf (mixLabel.get(), 0.35f));
    tools->addChild (makeLeaf (mixSlider.get(), 2.4f, 0.f));
    tools->addChild (makeLeaf (mixValue.get(), 0.4f));
    tools->addChild (makeLeaf (statusBarLabel.get(), 2.15f));
    layoutRoot->addChild (std::move (tools));

    auto body = makeRow (1.0f);
    body->innerMargin = 4;

    auto leftPanel = makeColumn (Config::kBodyKnobsWeight);
    leftPanel->innerMargin = 3;
    leftPanel->minWidth = 120;
    leftPanel->maxWidth = 168;
    auto channelLeaf = makeLeaf (inputChannelSwitch.get(), 0.f);
    channelLeaf->minHeight = Config::kActionRowHeight;
    channelLeaf->maxHeight = Config::kActionRowHeight;
    leftPanel->addChild (std::move (channelLeaf));
    for (int i = 0; i < Config::kNumUserParams; ++i)
        leftPanel->addChild (makeLeaf (paramComponents[(size_t) i].get(), 1.f));

    auto centerPanel = makeColumn (Config::kBodyEditorWeight);
    centerPanel->innerMargin = 3;
    auto actionRow = makeRow (0.0f);
    actionRow->innerMargin = 3;
    actionRow->minHeight = Config::kActionRowHeight;
    actionRow->maxHeight = Config::kActionRowHeight;
    actionRow->addChild (makeLeaf (workspaceGraphButton.get(), 0.7f));
    actionRow->addChild (makeLeaf (workspaceScriptButton.get(), 0.7f));
    auto editLeaf = makeLeaf (editSaveButton.get(), 0.7f);
    editLeaf->showHideEnabled = true;
    editLeaf->visibleWhen = [this] { return workspaceMode == WorkspaceScript; };
    actionRow->addChild (std::move (editLeaf));
    auto copyLeaf = makeLeaf (copyFormulaButton.get(), 0.55f);
    copyLeaf->showHideEnabled = true;
    copyLeaf->visibleWhen = [this] { return workspaceMode == WorkspaceScript; };
    auto optLeaf = makeLeaf (optimizeButton.get(), 0.7f);
    optLeaf->showHideEnabled = true;
    optLeaf->visibleWhen = [this] { return workspaceMode == WorkspaceScript; };
    actionRow->addChild (std::move (copyLeaf));
    actionRow->addChild (std::move (optLeaf));
    auto fontMinus = makeLeaf (editorFontMinusButton.get(), 0.5f);
    fontMinus->showHideEnabled = true;
    fontMinus->visibleWhen = [this] { return workspaceMode == WorkspaceScript; };
    auto fontPlus = makeLeaf (editorFontPlusButton.get(), 0.5f);
    fontPlus->showHideEnabled = true;
    fontPlus->visibleWhen = [this] { return workspaceMode == WorkspaceScript; };
    actionRow->addChild (std::move (fontMinus));
    actionRow->addChild (std::move (fontPlus));
    centerPanel->addChild (std::move (actionRow));

    auto graphLeaf = makeLeaf (graphCanvas.get(), 1.f);
    graphLeaf->showHideEnabled = true;
    graphLeaf->visibleWhen = [this] { return workspaceMode == WorkspaceGraph; };
    auto formulaLeaf = makeLeaf (formulaLiveDisplay.get(), 1.f);
    formulaLeaf->showHideEnabled = true;
    formulaLeaf->visibleWhen = [this] { return workspaceMode == WorkspaceScript; };
    centerPanel->addChild (std::move (graphLeaf));
    centerPanel->addChild (std::move (formulaLeaf));
    auto errLeaf = makeLeaf (errorLabel.get(), 0.0f);
    errLeaf->minHeight = 16;
    errLeaf->maxHeight = 18;
    centerPanel->addChild (std::move (errLeaf));

    body->addChild (std::move (leftPanel));
    body->addChild (std::move (centerPanel));
    layoutRoot->addChild (std::move (body));

    auto waveRow = makeRow (0.0f);
    waveRow->innerMargin = 4;
    waveRow->minHeight = Config::kScopeRowHeight;
    waveRow->maxHeight = Config::kScopeRowHeight + 24;
    {
        auto inLeaf = makeLeaf (inputDisplay.get(), 1.f);
        inLeaf->minHeight = Config::kScopeRowHeight;
        auto outLeaf = makeLeaf (outputDisplay.get(), 1.f);
        outLeaf->minHeight = Config::kScopeRowHeight;
        waveRow->addChild (std::move (inLeaf));
        waveRow->addChild (std::move (outLeaf));
    }
    layoutRoot->addChild (std::move (waveRow));

    updateTranslations();
    updateStatusBar();

    applyMotion (UiSettings::get().motion());
    setWorkspaceMode (WorkspaceGraph);
    applyUiScale();
    startTimerHz (30); // live formula values track knobs smoothly
}

NeuroKoreAudioProcessorEditor::~NeuroKoreAudioProcessorEditor()
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
    irOverlay.reset();
    licenseOverlay.reset();
    settingsOverlay.reset();
    audioProcessor.removeChangeListener(this);
    Localiser::getInstance().removeListener(this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::String NeuroKoreAudioProcessorEditor::getFormulaText() const
{
    return formulaInputEditor ? formulaInputEditor->getText() : juce::String();
}

void NeuroKoreAudioProcessorEditor::setFormulaText(const juce::String& text)
{
    if (formulaInputEditor)
        formulaInputEditor->setText (text);
    updateLiveFormulaView();
    syncGraphFromScript();
}

void NeuroKoreAudioProcessorEditor::applyEditorFontSize (float heightPt)
{
    editorFontHeight = juce::jlimit (Config::kMinEditorFontPt, Config::kMaxEditorFontPt, heightPt);
    UiSettings::get().setEditorFontPt (editorFontHeight);
    if (formulaInputEditor != nullptr)
        formulaInputEditor->setFontHeight (editorFontHeight);
    if (formulaLiveDisplay != nullptr)
        formulaLiveDisplay->setFontHeight (editorFontHeight);
    if (editorFontSizeLabel != nullptr)
        editorFontSizeLabel->setText (juce::String ((int) std::round (editorFontHeight)),
                                      juce::dontSendNotification);
}

void NeuroKoreAudioProcessorEditor::setFormulaEditMode(bool shouldEdit)
{
    editing = shouldEdit;
    if (formulaInputEditor)
    {
        formulaInputEditor->setReadOnly(! shouldEdit);
        formulaInputEditor->setVisible (shouldEdit && workspaceMode == WorkspaceScript);
        if (shouldEdit)
        {
            formulaInputEditor->setEditorColour(juce::TextEditor::backgroundColourId,
                                                NeuroKoreLookAndFeel::background());
            formulaInputEditor->setEditorColour(juce::TextEditor::textColourId,
                                                NeuroKoreLookAndFeel::brightText());
            formulaInputEditor->setEditorColour(juce::TextEditor::highlightColourId,
                                                NeuroKoreLookAndFeel::accent().withAlpha (0.35f));
            formulaInputEditor->setEditorColour(juce::CaretComponent::caretColourId,
                                                NeuroKoreLookAndFeel::accent());
            formulaInputEditor->setEditorColour(juce::TextEditor::outlineColourId,
                                                NeuroKoreLookAndFeel::accent().withAlpha (0.5f));
            // Editor overlays the live display bounds
            if (formulaLiveDisplay)
                formulaInputEditor->setBounds(formulaLiveDisplay->getBounds());
            formulaInputEditor->toFront(true);
        }
    }
    if (layoutRoot)
        ui::performLayout (*layoutRoot, chromeBounds());
    if (editSaveButton)
        editSaveButton->setButtonText(shouldEdit ? TRANS("SaveButton") : TRANS("EditButton"));
    if (! shouldEdit)
    {
        updateLiveFormulaView();
        syncGraphFromScript();
    }
}

void NeuroKoreAudioProcessorEditor::updateLiveFormulaView()
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

    const auto script = (editing && formulaInputEditor != nullptr)
                            ? formulaInputEditor->getText()
                            : audioProcessor.getScript();
    formulaLiveDisplay->setFormula(script);
    formulaLiveDisplay->refreshIrButtons();
    if (editing && formulaInputEditor != nullptr)
        formulaInputEditor->refreshIrButtons();
}

void NeuroKoreAudioProcessorEditor::timerCallback()
{
    if (! editing)
        updateLiveFormulaView();
    updateStatusBar();
    const bool live = audioProcessor.isLiveMode();
    if (backdrop && ! live)
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
    applyBypassMixLock();

#if ! defined (NEUROKORE_SKIP_LICENSE_ENFORCEMENT)
    if (Config::kEnableLicensing && audioProcessor.isDemoMixLocked())
        if (auto* dry = audioProcessor.apvts.getParameter (EffectParameters::dryWet))
            if (dry->getValue() > 0.f)
                dry->setValueNotifyingHost (0.f);
#endif

    for (auto& pc : paramComponents)
        if (pc && ! pc->isRenaming())
            pc->refreshValues();

    if (mixSlider && mixValue)
        mixValue->setText (juce::String (mixSlider->getValue() * 100.0, 0) + "%",
                           juce::dontSendNotification);

    // Combo readouts: append current selection into status so OS/Polisher always visible
    // (combo text itself already shows selection; status bar duplicates for HUD)
}

void NeuroKoreAudioProcessorEditor::updateStatusBar()
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
    const int latSm = audioProcessor.getLatencySamples();
    const float latMs = (srInt > 0) ? (1000.f * (float) latSm / (float) srInt) : 0.f;

    const bool cpuSafe = audioProcessor.isCpuProtectActive();
    const float cpuLoad = audioProcessor.getCpuLoad();
    const juce::String mode = cpuSafe ? "SAFE"
        : (bypassed ? "BYPASS"
                    : (audioProcessor.isLiveMode() ? "LIVE" : "STUDIO"));
    const int cpuPct = juce::jlimit (0, 999, (int) std::lround ((double) cpuLoad * 100.0));
    juce::String s;
    s << mode.paddedRight (' ', 6)
      << "  " << juce::String (cpuPct).paddedLeft (' ', 3) << "%"
      << "  " << juce::String (latMs, 1) << "ms"
      << "  " << juce::String (osFactor) << "x";
    if (lim)
        s << "  LIM";
    if (Config::kEnableLicensing && ! audioProcessor.isProductLicensed())
    {
        if (audioProcessor.isDemoMixLocked())
            s << "  DEMO";
        else
        {
            const int left = audioProcessor.demoSecondsRemaining();
            s << "  " << juce::String (left / 60).paddedLeft ('0', 2) << ":"
              << juce::String (left % 60).paddedLeft ('0', 2);
        }
    }

    juce::String tip;
    tip << mode << "   CPU " << cpuPct << "%   LAT " << latSm << " smp / "
        << juce::String (latMs, 2) << " ms   SR " << (srInt > 0 ? juce::String (srInt) : juce::String ("-"))
        << "   OS " << osFactor << "x   MIX " << juce::String ((int) std::lround ((double) mixPct)) << "%";
    statusBarLabel->setTooltip (tip);
    statusBarLabel->setText (s, juce::dontSendNotification);
    statusBarLabel->setColour (juce::Label::textColourId,
                               (lim || bypassed || cpuSafe)
                                   ? NeuroKoreLookAndFeel::ink()
                                   : NeuroKoreLookAndFeel::inkMuted());

    if (errorLabel != nullptr)
    {
        if (cpuSafe)
        {
            errorLabel->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
            errorLabel->setColour (juce::Label::backgroundColourId, NeuroKoreLookAndFeel::canvas());
            errorLabel->setText ("CPU overload — wet path paused. Retrying automatically. "
                                 "Lower oversampling if it keeps coming back.",
                                 juce::dontSendNotification);
        }
        else if (errorLabel->getText().startsWith ("CPU overload"))
            errorLabel->setText ({}, juce::dontSendNotification);
    }
}

void NeuroKoreAudioProcessorEditor::refreshParameterControls()
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
        const auto knobCol = NeuroKoreLookAndFeel::accent();
        if (nameEditors[i])
        {
            nameEditors[i]->setText(audioProcessor.getVariableName(i), juce::dontSendNotification);
            nameEditors[i]->setColour(juce::TextEditor::textColourId, knobCol);
            nameEditors[i]->setColour(juce::TextEditor::outlineColourId, knobCol.withAlpha(0.65f));
        }
        if (paramComponents[i] && ! paramComponents[i]->isRenaming())
        {
            paramComponents[i]->setAliasName(audioProcessor.getVariableName(i));
            paramComponents[i]->setAccentColour(knobCol);
            std::vector<juce::String> notes;
            for (const auto& pd : paramInfo)
                if (pd.isNote && pd.alias.length() == 1 && (pd.alias[0] - 'a') == i)
                    notes = pd.noteLabels;
            if (! notes.empty())
            {
                paramComponents[i]->setNoteGrid (std::move (notes));
            }
            else
            {
                paramComponents[i]->setNoteGrid ({});
                if (hasMap[(size_t) i])
                    paramComponents[i]->setMappedRange(mapMin[(size_t) i], mapMax[(size_t) i]);
                else
                    paramComponents[i]->setMappedRange(0.f, 1.f);
            }
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

void NeuroKoreAudioProcessorEditor::syncFromProcessor()
{
    cancelWindowAssemble();
    audioProcessor.resolvePresetNameFromScript();
    // Preset wins. Stay in Graph or Script — do not bounce the workspace.
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
                                       name.isNotEmpty() ? NeuroKoreLookAndFeel::brightText()
                                                         : NeuroKoreLookAndFeel::mutedText());
        currentPresetLabel->setColour (juce::Label::backgroundColourId,
                                       NeuroKoreLookAndFeel::accent().withAlpha (
                                           name.isNotEmpty() ? 0.22f : 0.10f));
    }

    applyBypassMixLock();

    if (errorLabel)
        errorLabel->setText({}, juce::dontSendNotification);

    refreshLicenseButton();
    updateStatusBar();
    syncGraphFromScript();
    if (layoutRoot)
        ui::performLayout (*layoutRoot, chromeBounds());
    repaint();
}

void NeuroKoreAudioProcessorEditor::applyBypassMixLock() noexcept
{
    const bool locked = bypassButton != nullptr && bypassButton->getToggleState();
    if (mixSlider)
    {
        mixSlider->setEnabled (! locked);
        mixSlider->setTooltip (locked ? "Mix locked while Bypass is on"
                                      : "Mix");
        if (locked && mixSlider->getValue() > 0.001)
            mixSlider->setValue (0.0, juce::dontSendNotification);
    }
    if (locked)
    {
        if (auto* p = audioProcessor.apvts.getParameter (EffectParameters::dryWet))
            if (p->getValue() > 0.f)
                p->setValueNotifyingHost (0.f);
    }
    if (bypassButton)
        bypassButton->setButtonText (locked ? "BYPASSED" : "BYPASS");
}

void NeuroKoreAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    syncFromProcessor();
}

void NeuroKoreAudioProcessorEditor::updateTranslations()
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
    if (presetPrevButton)
        presetPrevButton->setTooltip ("Previous preset");
    if (presetNextButton)
        presetNextButton->setTooltip ("Next preset");
    if (functionsButton)
        functionsButton->setButtonText(TRANS("Functions"));
    if (stagesButton)
        stagesButton->setButtonText ("Stages");
    if (bypassButton)
        bypassButton->setButtonText (bypassButton->getToggleState() ? "BYPASSED" : "BYPASS");
    if (editorFontLabel)
        editorFontLabel->setText ("Text", juce::dontSendNotification);
    if (polisherLabel)
        polisherLabel->setText(TRANS("PolisherLabel"), juce::dontSendNotification);
    if (helpButton)
        helpButton->setButtonText(TRANS("HelpButton"));
    refreshLicenseButton();
    if (oversamplingLabel)
        oversamplingLabel->setText(TRANS("OversamplingLabel"), juce::dontSendNotification);
}

void NeuroKoreAudioProcessorEditor::paintHudChrome (juce::Graphics& g)
{
    // Frame panels with HUD corners ONLY — never fill opaque over OpenGL children.
    // Children live in design-space on scaledRoot; paint() is host-window space.
    const auto toHost = [this] (const juce::Component& c) -> juce::Rectangle<float>
    {
        auto r = c.getBounds().toFloat().expanded (2.f);
        if (scaledRoot != nullptr)
            r = r.transformedBy (scaledRoot->getTransform());
        return r;
    };

    struct PanelTag { const juce::Component* c; const char* tag; };
    const PanelTag panels[] = {
        { formulaLiveDisplay.get(),  "SCRIPT" },
        { formulaInputEditor.get(),  "EDIT" },
        { graphCanvas.get(),         "GRAPH" },
        { inputDisplay.get(),        "IN" },
        { outputDisplay.get(),       "OUT" },
    };

    for (const auto& p : panels)
    {
        if (p.c == nullptr || ! p.c->isVisible() || p.c->getWidth() < 4)
            continue;
        auto r = toHost (*p.c);
        g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.55f));
        NeuroKoreLookAndFeel::drawHudFrame (g, r, p.tag);

        // Live LED on frame corner
        const bool hot = p.c == outputDisplay.get()
                             ? audioProcessor.isLimiterActive()
                             : (lookAndFeel.peakPulse > 0.15f);
        g.setColour (hot ? NeuroKoreLookAndFeel::accent()
                         : NeuroKoreLookAndFeel::accent().withAlpha (0.25f));
        g.fillEllipse (r.getRight() - 10.f, r.getY() + 4.f, 5.f, 5.f);
    }

    if (errorLabel != nullptr && errorLabel->getText().startsWith ("CPU overload"))
    {
        const auto r = toHost (*errorLabel).toNearestInt();
        g.setColour (NeuroKoreLookAndFeel::warningMark());
        g.fillRect (r.getX(), r.getY(), 3, r.getHeight());
    }

    // Bottom threat / link strip
    g.setColour (juce::Colour (0xff080000));
    g.fillRect (0, getHeight() - 3, getWidth(), 3);
    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.5f + 0.5f * lookAndFeel.peakPulse));
    const int barW = (int) (getWidth() * juce::jlimit (0.05f, 1.f, lookAndFeel.peakPulse));
    g.fillRect (0, getHeight() - 3, barW, 3);
}

void NeuroKoreAudioProcessorEditor::paint (juce::Graphics& g)
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

void NeuroKoreAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (! anyOverlayShowing()
        && workspaceMode == WorkspaceGraph
        && graphCanvas != nullptr && graphCanvas->isShowing())
    {
        auto inner = getLocalArea (graphCanvas.get(), graphCanvas->getLocalBounds());
        juce::Path clip;
        clip.addRectangle (inner.toFloat());
        for (auto& pc : paramComponents)
        {
            if (pc == nullptr || ! pc->isShowing())
                continue;
            clip.addRectangle (getLocalArea (pc.get(), pc->getLocalBounds()).toFloat());
        }
        g.saveState();
        g.reduceClipRegion (clip);
        graphCanvas->paintKnobCables (g, *this);
        g.restoreState();
    }

    if (cyberDirector.getState().motion == CyberMotion::Full)
        CyberChrome::drawCrtGlow (g, getLocalBounds(),
                                  cyberDirector.getState().timeSec,
                                  lookAndFeel.peakPulse);
}

void NeuroKoreAudioProcessorEditor::visibilityChanged()
{
    const auto motion = UiSettings::get().motion();
    cyberDirector.setVisible (isShowing() && motion != CyberMotion::Off);
    if (isShowing() && motion == CyberMotion::Full)
        startWindowAssemble();
}

int NeuroKoreAudioProcessorEditor::overlayTopInset() const noexcept
{
    return Config::kOverlayTopChromeDesign;
}

void NeuroKoreAudioProcessorEditor::applyOverlayMotion (ModalOverlay& overlay)
{
    overlay.setMotion (cyberDirector.getState().motion);
    overlay.setTopInset (overlayTopInset());
}

void NeuroKoreAudioProcessorEditor::reportScriptIssue (const juce::String& message)
{
    if (errorLabel != nullptr)
    {
        errorLabel->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::error());
        errorLabel->setText (message, juce::dontSendNotification);
    }
    if (formulaInputEditor != nullptr)
        formulaInputEditor->setLineError (firstScriptErrorLine (message), message);
}

void NeuroKoreAudioProcessorEditor::dismissOverlayNow (std::unique_ptr<ModalOverlay>& overlay)
{
    if (overlay == nullptr)
        return;
    overlay->skipToEnd();
    overlay.reset();
    syncGlCover();
}

void NeuroKoreAudioProcessorEditor::layoutOpenOverlays()
{
    const int inset = overlayTopInset();
    auto fit = [inset] (std::unique_ptr<ModalOverlay>& o)
    {
        if (o != nullptr && o->isShowing())
        {
            o->setTopInset (inset);
            o->fitToParent();
        }
    };
    fit (presetOverlay);
    fit (functionsOverlay);
    fit (stagesOverlay);
    fit (validationOverlay);
    fit (irOverlay);
    fit (licenseOverlay);
    fit (settingsOverlay);
}

bool NeuroKoreAudioProcessorEditor::anyOverlayShowing() const noexcept
{
    auto on = [] (const std::unique_ptr<ModalOverlay>& o) noexcept
    {
        return o != nullptr && o->isShowing();
    };
    return on (presetOverlay) || on (functionsOverlay) || on (stagesOverlay)
        || on (validationOverlay) || on (irOverlay) || on (licenseOverlay)
        || on (settingsOverlay);
}

void NeuroKoreAudioProcessorEditor::syncGlCover()
{
    if (loudnessMeter != nullptr)
        loudnessMeter->setCoveredByOverlay (anyOverlayShowing());
}

void NeuroKoreAudioProcessorEditor::cancelWindowAssemble()
{
    if (! assemblingWindows)
        return;
    assemblingWindows = false;
    assemblePlayed = true;
    assembleVblank = {};
    if (layoutRoot)
        ui::performLayout (*layoutRoot, chromeBounds());
}

void NeuroKoreAudioProcessorEditor::startWindowAssemble()
{
    if (UiSettings::get().motion() != CyberMotion::Full || assemblePlayed || assemblingWindows)
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

void NeuroKoreAudioProcessorEditor::captureAssembleTargets()
{
    assembleSlots.clear();
    auto add = [this] (juce::Component* c, float delay)
    {
        if (c == nullptr || ! c->isVisible() || c->getWidth() < 8 || c->getHeight() < 8)
            return;
        assembleSlots.push_back ({ c, c->getBounds(), randomClipReveal (assembleRng), delay });
    };

    if (workspaceMode == WorkspaceGraph)
        add (graphCanvas.get(), 0.00f);
    else
        add (formulaLiveDisplay.get(), 0.00f);
    add (inputDisplay.get(),       0.07f);
    add (outputDisplay.get(),      0.10f);
    for (size_t i = 0; i < paramComponents.size(); ++i)
        add (paramComponents[i].get(), 0.02f + 0.025f * (float) i);
}

void NeuroKoreAudioProcessorEditor::applyWindowAssemble()
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

void NeuroKoreAudioProcessorEditor::onAssembleVBlank (double nowSec)
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

juce::Rectangle<int> NeuroKoreAudioProcessorEditor::chromeBounds() const
{
    auto r = juce::Rectangle<int> (0, 0, Config::kUiDesignWidth, Config::kUiDesignHeight);
    r.removeFromTop (Config::kHudHeaderHeight);
    return r;
}

void NeuroKoreAudioProcessorEditor::applyUiScale()
{
    const float s = UiSettings::get().uiScaleFactor();
    setSize (juce::roundToInt ((float) Config::kUiDesignWidth * s),
             juce::roundToInt ((float) Config::kUiDesignHeight * s));
}

void NeuroKoreAudioProcessorEditor::applyMotion (CyberMotion motion)
{
    UiSettings::get().setMotion (motion);
    cyberDirector.setMotion (motion);
    cyberDirector.setVisible (isShowing() && motion != CyberMotion::Off);
    if (motion != CyberMotion::Full)
    {
        assemblingWindows = false;
        assemblePlayed = true;
        assembleVblank = {};
        if (layoutRoot)
            ui::performLayout (*layoutRoot, chromeBounds());
    }
}

bool NeuroKoreAudioProcessorEditor::isLiveFormulaVisible() const
{
    return workspaceMode == WorkspaceScript
        && formulaLiveDisplay != nullptr
        && formulaLiveDisplay->isVisible()
        && (formulaInputEditor == nullptr || ! formulaInputEditor->isVisible());
}

bool NeuroKoreAudioProcessorEditor::isFormulaEditorVisible() const
{
    return formulaInputEditor != nullptr && formulaInputEditor->isVisible();
}

void NeuroKoreAudioProcessorEditor::setWorkspaceMode (int mode)
{
    const int next = (mode == WorkspaceGraph) ? WorkspaceGraph : WorkspaceScript;
    if (next != workspaceMode)
    {
        if (workspaceMode == WorkspaceGraph && next == WorkspaceScript)
        {
            if (! commitGraphToScript())
            {
                if (workspaceGraphButton)
                    workspaceGraphButton->setToggleState (true, juce::dontSendNotification);
                return;
            }
        }
        else if (workspaceMode == WorkspaceScript && next == WorkspaceGraph)
        {
            if (! commitScriptToGraph())
            {
                if (workspaceScriptButton)
                    workspaceScriptButton->setToggleState (true, juce::dontSendNotification);
                return;
            }
        }
    }

    workspaceMode = next;
    cancelWindowAssemble();
    if (workspaceGraphButton)
        workspaceGraphButton->setToggleState (workspaceMode == WorkspaceGraph, juce::dontSendNotification);
    if (workspaceScriptButton)
        workspaceScriptButton->setToggleState (workspaceMode == WorkspaceScript, juce::dontSendNotification);
    if (workspaceMode == WorkspaceGraph)
        setFormulaEditMode (false);
    if (layoutRoot)
        ui::performLayout (*layoutRoot, chromeBounds());
    if (formulaInputEditor && formulaLiveDisplay)
        formulaInputEditor->setBounds (formulaLiveDisplay->getBounds());
    if (workspaceMode == WorkspaceScript)
    {
        updateLiveFormulaView();
        const auto script = formulaInputEditor != nullptr
                                ? formulaInputEditor->getText()
                                : audioProcessor.getScript();
        syncParamNamesFromScript (script);
    }
    repaint();
}

bool NeuroKoreAudioProcessorEditor::commitGraphToScript()
{
    if (graphCanvas == nullptr || ! graphCanvas->hasValidGraph())
        return true;
    const auto emitted = graphCanvas->getEmittedScript();
    if (emitted.isEmpty())
        return true;

    dsl::GraphDocument cur, next;
    juce::String parseErr;
    const bool same = dsl::parse (audioProcessor.getScript(), cur, parseErr)
                   && dsl::parse (emitted, next, parseErr)
                   && dsl::semanticallyEqual (cur, next);

    if (! same)
    {
        juce::String err;
        if (! audioProcessor.applyFormula (emitted, err, false))
        {
            reportScriptIssue (err.isNotEmpty() ? err : "Graph could not be written to the script");
            return false;
        }
    }
    if (formulaInputEditor)
        formulaInputEditor->setText (same ? audioProcessor.getScript() : emitted);
    updateLiveFormulaView();
    return true;
}

bool NeuroKoreAudioProcessorEditor::commitScriptToGraph()
{
    const auto script = (editing && formulaInputEditor != nullptr)
                            ? formulaInputEditor->getText()
                            : audioProcessor.getScript();
    if (editing)
    {
        juce::String err;
        if (! audioProcessor.applyFormula (script, err, false))
        {
            reportScriptIssue (err.isNotEmpty() ? err : "Fix the script before opening Graph");
            return false;
        }
        setFormulaEditMode (false);
    }
    if (graphCanvas != nullptr)
        graphCanvas->setScript (audioProcessor.getScript());
    return true;
}

void NeuroKoreAudioProcessorEditor::applyKnobDisplayName (int index, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (index, Config::kNumUserParams))
        return;
    audioProcessor.setVariableName (index, name);
    if (paramComponents[(size_t) index] && ! paramComponents[(size_t) index]->isRenaming())
        paramComponents[(size_t) index]->setAliasName (name);
    if (nameEditors[(size_t) index])
        nameEditors[(size_t) index]->setText (name, juce::dontSendNotification);
}

void NeuroKoreAudioProcessorEditor::syncParamNamesFromScript (const juce::String& script)
{
    juce::String names[Config::kNumUserParams];
    dsl::collectParamDisplayNames (script, names, Config::kNumUserParams);
    for (int i = 0; i < Config::kNumUserParams; ++i)
        if (names[i].isNotEmpty())
            applyKnobDisplayName (i, names[i]);
}

void NeuroKoreAudioProcessorEditor::writeParamNameToScript (int index, const juce::String& name)
{
    const auto src = (formulaInputEditor != nullptr && (editing || workspaceMode == WorkspaceScript))
                         ? formulaInputEditor->getText()
                         : audioProcessor.getScript();
    const auto next = dsl::rewriteParamDisplayName (src, index, name);
    if (next == src)
        return;

    ignoreScriptNameSync = true;
    if (formulaInputEditor != nullptr)
        formulaInputEditor->setText (next);
    ignoreScriptNameSync = false;

    if (workspaceMode == WorkspaceGraph)
    {
        juce::String err;
        audioProcessor.setFormula (next, err, false);
        syncGraphFromScript();
    }
    else if (! editing)
    {
        juce::String err;
        audioProcessor.setFormula (next, err, false);
    }
}

void NeuroKoreAudioProcessorEditor::syncGraphFromScript()
{
    if (graphCanvas != nullptr)
        graphCanvas->setScript (audioProcessor.getScript());
}

void NeuroKoreAudioProcessorEditor::resized()
{
    if (backdrop != nullptr)
        backdrop->setBounds (getLocalBounds());

    const float fit = juce::jmin ((float) getWidth() / (float) Config::kUiDesignWidth,
                                  (float) getHeight() / (float) Config::kUiDesignHeight);
    if (scaledRoot != nullptr)
    {
        scaledRoot->setTransform ({});
        scaledRoot->setBounds (0, 0, Config::kUiDesignWidth, Config::kUiDesignHeight);
        scaledRoot->setTransform (juce::AffineTransform::scale (fit));
    }

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

    layoutOpenOverlays();

    if (statusBarLabel)
    {
        statusBarLabel->setFont (NeuroKoreLookAndFeel::monoFont (11.f));
        statusBarLabel->setColour (juce::Label::textColourId,
                                   NeuroKoreLookAndFeel::inkMuted());
    }
    if (mixLabel)
    {
        mixLabel->setJustificationType (juce::Justification::centredLeft);
        mixLabel->setFont (NeuroKoreLookAndFeel::monoFont (12.f));
        mixLabel->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
    }
    for (auto* lbl : { oversamplingLabel.get(), polisherLabel.get() })
    {
        if (lbl != nullptr)
        {
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(NeuroKoreLookAndFeel::brandFont(11.f, true));
            lbl->setColour(juce::Label::textColourId, NeuroKoreLookAndFeel::mutedText());
        }
    }
    if (mixValue)
    {
        mixValue->setJustificationType (juce::Justification::centredRight);
        mixValue->setFont (NeuroKoreLookAndFeel::monoFont (13.f));
        mixValue->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
    }
    if (currentPresetLabel)
    {
        currentPresetLabel->setFont (NeuroKoreLookAndFeel::brandFont (Config::kPresetChipFontPt, true));
        currentPresetLabel->setColour (juce::Label::backgroundColourId,
                                       NeuroKoreLookAndFeel::accent().withAlpha (0.18f));
        currentPresetLabel->setJustificationType (juce::Justification::centred);
    }
    if (errorLabel)
    {
        errorLabel->setJustificationType(juce::Justification::centredLeft);
        errorLabel->setFont (NeuroKoreLookAndFeel::monoFont (12.f));
        errorLabel->setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
        errorLabel->setColour (juce::Label::backgroundColourId, NeuroKoreLookAndFeel::canvas());
    }
}

bool NeuroKoreAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    auto applyUndoRedo = [this](bool isUndo) -> bool
    {
        bool ok = isUndo ? audioProcessor.undo() : audioProcessor.redo();
        if (ok)
        {
            if (formulaInputEditor)
                formulaInputEditor->setText (audioProcessor.getScript());
            refreshParameterControls();
            syncGraphFromScript();
            updateLiveFormulaView();
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

void NeuroKoreAudioProcessorEditor::showPresetOverlay()
{
    dismissOverlayNow (presetOverlay);
    auto content = std::make_unique<PresetContentComponent>(audioProcessor, lookAndFeel);
    auto* ptr = content.get();

    presetOverlay = std::make_unique<ModalOverlay>();
    presetOverlay->setMode(OverlayMode::Closable);
    presetOverlay->setTitle ("Preset Explorer");
    presetOverlay->setPreferredContentSize (0, 0);
    presetOverlay->setContent(std::move(content));
    applyOverlayMotion (*presetOverlay);
    presetOverlay->show (*scaledRoot);
    syncGlCover();
    presetOverlay->onClose = [this] { presetOverlay.reset(); syncGlCover(); };

    auto refreshAfterPreset = [this]
    {
        cancelWindowAssemble();
        setFormulaEditMode (false);
        if (formulaInputEditor)
            formulaInputEditor->setText (audioProcessor.getScript());
        updateLiveFormulaView();
        syncGraphFromScript();
        syncFromProcessor();

        const auto q = audioProcessor.analyseFormulaQuality (audioProcessor.getScript());
        if (errorLabel)
        {
            errorLabel->setColour (juce::Label::textColourId,
                                   q.ok ? (q.warnings.isEmpty() ? juce::Colour (0xff7dcea0)
                                                                : juce::Colour (0xfff4d03f))
                                        : NeuroKoreLookAndFeel::error());
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

void NeuroKoreAudioProcessorEditor::showFunctionsOverlay()
{
    dismissOverlayNow (functionsOverlay);
    auto content = std::make_unique<FunctionsContentComponent>(audioProcessor);
    auto* ptr = content.get();

    functionsOverlay = std::make_unique<ModalOverlay>();
    functionsOverlay->setMode(OverlayMode::Closable);
    functionsOverlay->setTitle(TRANS("Functions"));
    functionsOverlay->setPreferredContentSize (1080, 740);
    functionsOverlay->setContent(std::move(content));
    applyOverlayMotion (*functionsOverlay);
    functionsOverlay->show (*scaledRoot);
    syncGlCover();

    ptr->onInsert = [this](const juce::String& text)
    {
        if (formulaInputEditor)
            formulaInputEditor->insertTextAtCaret(text);
    };
    ptr->onClose = [this]{ functionsOverlay.reset(); syncGlCover(); };
    functionsOverlay->onClose = [this]{ functionsOverlay.reset(); syncGlCover(); };
}

void NeuroKoreAudioProcessorEditor::hideFunctionsOverlay()
{
    if (functionsOverlay)
        functionsOverlay->requestClose();
}

void NeuroKoreAudioProcessorEditor::showStagesOverlay()
{
    dismissOverlayNow (stagesOverlay);
    auto content = std::make_unique<StagesContentComponent>(audioProcessor);
    auto* ptr = content.get();

    stagesOverlay = std::make_unique<ModalOverlay>();
    stagesOverlay->setMode(OverlayMode::Closable);
    stagesOverlay->setTitle(TRANS("StagesTitle"));
    stagesOverlay->setPreferredContentSize (880, 520);
    stagesOverlay->setContent(std::move(content));
    applyOverlayMotion (*stagesOverlay);
    stagesOverlay->show (*scaledRoot);
    syncGlCover();

    ptr->onClose = [this] { stagesOverlay.reset(); syncGlCover(); };
    ptr->onOpenIr = [this] (juce::String slot)
    {
        if (slot.isNotEmpty())
            showIrOverlay (slot);
    };
    stagesOverlay->onClose = [this] { stagesOverlay.reset(); syncGlCover(); };
}

void NeuroKoreAudioProcessorEditor::showIrOverlay (const juce::String& slot)
{
    dismissOverlayNow (irOverlay);
    auto content = std::make_unique<IrPanelComponent> (audioProcessor, slot);
    auto* ptr = content.get();
    irOverlay = std::make_unique<ModalOverlay>();
    irOverlay->setMode (OverlayMode::Closable);
    irOverlay->setTitle ("IR  " + slot);
    irOverlay->setPreferredContentSize (720, 420);
    irOverlay->setContent (std::move (content));
    applyOverlayMotion (*irOverlay);
    irOverlay->show (*scaledRoot);
    syncGlCover();
    auto refreshIrCaptions = [this]
    {
        if (formulaLiveDisplay)
        {
            formulaLiveDisplay->setFormula (audioProcessor.getScript());
            formulaLiveDisplay->refreshIrButtons();
        }
        if (formulaInputEditor)
            formulaInputEditor->refreshIrButtons();
    };
    ptr->onClose = [this, refreshIrCaptions]
    {
        irOverlay.reset();
        syncGlCover();
        refreshIrCaptions();
    };
    irOverlay->onClose = [this, refreshIrCaptions]
    {
        irOverlay.reset();
        syncGlCover();
        refreshIrCaptions();
    };
}

void NeuroKoreAudioProcessorEditor::hideIrOverlay()
{
    if (irOverlay)
        irOverlay->requestClose();
}

void NeuroKoreAudioProcessorEditor::refreshLicenseButton()
{
    if (licenseButton == nullptr)
        return;
    licenseButton->setButtonText ("License");
    if (audioProcessor.isProductLicensed())
    {
        const auto who = audioProcessor.licensedEmail();
        licenseButton->setTooltip (who.isNotEmpty()
            ? ("Licensed to " + who)
            : "Licensed — click for details");
    }
    else
    {
        licenseButton->setTooltip ("Import a signed .lic file");
    }
}

void NeuroKoreAudioProcessorEditor::promptImportLicense()
{
    auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    licenseChooser = std::make_unique<juce::FileChooser> (
        "Import NeuroKore license", start, "*.lic");
    constexpr int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;
    licenseChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto src = fc.getResult();
        if (! src.existsAsFile())
            return;
        if (audioProcessor.importProductLicense (src))
        {
            refreshLicenseButton();
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
}

void NeuroKoreAudioProcessorEditor::showLicenseInfoOverlay()
{
    dismissOverlayNow (licenseOverlay);
    auto content = std::make_unique<LicenseInfoComponent> (
        audioProcessor.licensedEmail(), audioProcessor.licensedIssued());
    auto* ptr = content.get();
    licenseOverlay = std::make_unique<ModalOverlay>();
    licenseOverlay->setMode (OverlayMode::Closable);
    licenseOverlay->setTitle ("License");
    licenseOverlay->setPreferredContentSize (520, 260);
    licenseOverlay->setContent (std::move (content));
    applyOverlayMotion (*licenseOverlay);
    licenseOverlay->show (*scaledRoot);
    syncGlCover();
    ptr->onClose = [this]
    {
        licenseOverlay.reset();
        syncGlCover();
    };
    ptr->onReplace = [this]
    {
        licenseOverlay.reset();
        syncGlCover();
        promptImportLicense();
    };
    licenseOverlay->onClose = [this]
    {
        licenseOverlay.reset();
        syncGlCover();
    };
}

void NeuroKoreAudioProcessorEditor::hideLicenseOverlay()
{
    if (licenseOverlay)
        licenseOverlay->requestClose();
}

void NeuroKoreAudioProcessorEditor::showSettingsOverlay()
{
    dismissOverlayNow (settingsOverlay);
    auto content = std::make_unique<SettingsContentComponent>();
    auto* ptr = content.get();
    ptr->setFontPt (editorFontHeight);
    settingsOverlay = std::make_unique<ModalOverlay>();
    settingsOverlay->setMode (OverlayMode::Closable);
    settingsOverlay->setTitle ("Settings");
    settingsOverlay->setPreferredContentSize (560, 560);
    settingsOverlay->setContent (std::move (content));
    applyOverlayMotion (*settingsOverlay);
    settingsOverlay->show (*scaledRoot);
    syncGlCover();
    ptr->onMotionChanged = [this] (CyberMotion m) { applyMotion (m); };
    ptr->onLiveModeChanged = [this] (bool live)
    {
        audioProcessor.setLiveMode (live);
        if (liveButton != nullptr)
        {
            liveButton->setToggleState (live, juce::dontSendNotification);
            liveButton->setButtonText (live ? "LIVE" : "STUDIO");
        }
        updateStatusBar();
    };
    ptr->onScaleChanged = [this] (int)
    {
        applyUiScale();
    };
    ptr->onFontStep = [this, ptr] (int delta)
    {
        applyEditorFontSize (editorFontHeight + (float) delta);
        ptr->setFontPt (editorFontHeight);
    };
    ptr->onLicense = [this]
    {
        if (settingsOverlay != nullptr)
            settingsOverlay->requestClose();
        if (audioProcessor.isProductLicensed())
            showLicenseInfoOverlay();
        else
            promptImportLicense();
    };
    ptr->onHelp = [this]
    {
        if (settingsOverlay != nullptr)
            settingsOverlay->requestClose();
        if (helpButton != nullptr)
            helpButton->triggerClick();
    };
    ptr->onClose = [this]
    {
        if (settingsOverlay != nullptr)
            settingsOverlay->requestClose();
    };
    settingsOverlay->onClose = [this]
    {
        settingsOverlay.reset();
        syncGlCover();
    };
}

void NeuroKoreAudioProcessorEditor::hideSettingsOverlay()
{
    if (settingsOverlay)
        settingsOverlay->requestClose();
}

void NeuroKoreAudioProcessorEditor::hideStagesOverlay()
{
    if (stagesOverlay)
        stagesOverlay->requestClose();
}
void NeuroKoreAudioProcessorEditor::hidePresetOverlay()
{
    if (presetOverlay)
        presetOverlay->requestClose();
}

void NeuroKoreAudioProcessorEditor::validateAndOverlay(const juce::String& expr)
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
    validationOverlay->show (*scaledRoot);
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
            reportScriptIssue (qualityHint + "  -  " + q.errors.joinIntoString ("; "));
        }
        else
        {
            reportScriptIssue (err.isNotEmpty() ? err
                                                : (qualityHint + "  -  validation failed"));
        }
    };
}





