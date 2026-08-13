/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <atomic>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include "PluginLookAndFeel.h"
#include "../utils/FormulaHelper.h"
#include "FormulaDisplayComponent.h"
#include "WaveformDisplayComponent.h"
#include "DslTerminalEditor.h"
#include "LoudnessMeterComponent.h"
#include "custom/ParameterComponent.h"
#include "custom/InputChannelSwitch.h"
#include "custom/CyberMixSlider.h"
#include "custom/BrandLockup.h"
#include "../utils/Localiser.h"
#include "ModalOverlay.h"
#include "ValidationContentComponent.h"
#include "PresetContentComponent.h"
#include "FunctionsContentComponent.h"
#include "StagesContentComponent.h"
#include "WeightedLayout.h"
#include "fx/CyberFxDirector.h"
#include "fx/CyberBackdropComponent.h"
#include "fx/CyberClip.h"


class ParameterSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (onRightClick) onRightClick();
            return;
        }
        juce::Slider::mouseDown(e);
    }
};


//==============================================================================
/**
*/
class NeuroCoreAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       private Localiser::Listener,
                                       private juce::ChangeListener,
                                       private juce::Timer
{
public:
    NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor&);
    ~NeuroCoreAudioProcessorEditor() override;

    juce::String getFormulaText() const;
    void setFormulaText(const juce::String& text);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void visibilityChanged() override;



private:
    void refreshParameterControls();
    void syncFromProcessor();
    void updateLiveFormulaView();
    void updateStatusBar();
    void paintHudChrome (juce::Graphics& g);
    void applyOverlayMotion (ModalOverlay& overlay);
    void dismissOverlayNow (std::unique_ptr<ModalOverlay>& overlay);
    void syncGlCover();
    juce::Rectangle<int> chromeBounds() const;
    void startWindowAssemble();
    void captureAssembleTargets();
    void applyWindowAssemble();
    void onAssembleVBlank (double nowSec);
    void setFormulaEditMode(bool shouldEdit);
    void applyEditorFontSize (float heightPt);
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    /// Updates all text labels after language change.
    void updateTranslations();
    void languageChanged() override { updateTranslations(); }
    float editorFontHeight { Config::kDefaultEditorFontPt };
    NeuroCoreAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::unique_ptr<ui::LayoutNode> layoutRoot;

    std::array<std::unique_ptr<ui::ParameterComponent>, Config::kNumUserParams> paramComponents;
    std::array<std::unique_ptr<juce::TextEditor>, Config::kNumUserParams>        nameEditors;
    std::unique_ptr<BrandLockup>         brandLockup;
    std::unique_ptr<juce::Label>         statusBarLabel; // LAT / SR / LIVE
    std::unique_ptr<juce::TextButton>    audioSettingsButton;
    std::unique_ptr<juce::TextButton>    helpButton;
    std::unique_ptr<juce::Label>         editorFontLabel;
    std::unique_ptr<juce::TextButton>    editorFontMinusButton;
    std::unique_ptr<juce::TextButton>    editorFontPlusButton;
    std::unique_ptr<juce::Label>         editorFontSizeLabel;
    std::unique_ptr<juce::TextButton>    presetsButton;
    std::unique_ptr<juce::ComboBox>      quickTemplateBox;
    std::unique_ptr<juce::Label>         quickTemplateLabel;
    std::unique_ptr<juce::TextButton>    bypassButton; // cyber toggle (text button styled)
    std::unique_ptr<juce::TextButton>    functionsButton;
    std::unique_ptr<juce::TextButton>    stagesButton;
    std::unique_ptr<CyberMixSlider>      mixSlider;
    std::unique_ptr<juce::Label>         mixLabel;
    std::unique_ptr<juce::Label>         mixValue;
    std::unique_ptr<juce::Label>         currentPresetLabel;
    std::unique_ptr<InputChannelSwitch>  inputChannelSwitch;
    std::unique_ptr<juce::ComboBox>      polisherBox;
    std::unique_ptr<juce::Label>         polisherLabel;
    std::unique_ptr<juce::ComboBox>      oversamplingBox;
    std::unique_ptr<juce::Label>         oversamplingLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>       polisherAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>       oversamplingAttachment;

    // Middle column editors
    std::unique_ptr<DslTerminalEditor>      formulaInputEditor;
    std::unique_ptr<FormulaDisplayComponent> formulaLiveDisplay;
    std::unique_ptr<juce::TextButton>       optimizeButton;
    std::unique_ptr<juce::TextButton>       copyFormulaButton;
    std::unique_ptr<juce::TextButton>       editSaveButton;
    std::unique_ptr<juce::Label>            errorLabel;
    bool                                    editing { false };
    float                                   mixBeforeBypass { 1.0f };

    NeuroCoreLookAndFeel lookAndFeel;
    CyberFxDirector cyberDirector;
    std::unique_ptr<CyberBackdropComponent> backdrop;
    struct WindowAssemble
    {
        juce::Component* c = nullptr;
        juce::Rectangle<int> target;
        ClipReveal type = ClipReveal::SystemBoot;
        float delaySec = 0.f;
    };
    std::vector<WindowAssemble> assembleSlots;
    bool assemblingWindows { false };
    bool assemblePlayed { false };
    float assembleElapsed { 0.f };
    double assembleStamp { 0.0 };
    juce::VBlankAttachment assembleVblank;
    juce::Random assembleRng { 0x41535345 };
    class LogoGlitchListener : public juce::MouseListener
    {
    public:
        CyberFxDirector* director = nullptr;
        juce::Random rng { 0x4c4f474f };
        void mouseEnter (const juce::MouseEvent&) override
        {
            if (director != nullptr)
                director->triggerGlitch (0.35f, rng.nextInt());
        }
    };
    LogoGlitchListener logoGlitch;
    std::unique_ptr<WaveformDisplayComponent> inputDisplay;
    std::unique_ptr<WaveformDisplayComponent> outputDisplay;
    std::unique_ptr<LoudnessMeterComponent>   loudnessMeter;
    std::unique_ptr<ModalOverlay>            presetOverlay;
    std::unique_ptr<ModalOverlay>            functionsOverlay;
    std::unique_ptr<ModalOverlay>            stagesOverlay;
    std::unique_ptr<ModalOverlay>            validationOverlay;

    void showPresetOverlay();
    void hidePresetOverlay();
    void showFunctionsOverlay();
    void hideFunctionsOverlay();
    void showStagesOverlay();
    void hideStagesOverlay();
    void validateAndOverlay(const juce::String& expr);

    //melatonin::Inspector inspector{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessorEditor)
};



