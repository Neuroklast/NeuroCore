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
#include "../utils/Localiser.h"
#include "ModalOverlay.h"
#include "ValidationContentComponent.h"
#include "PresetContentComponent.h"
#include "FunctionsContentComponent.h"
#include "StagesContentComponent.h"
#include "WeightedLayout.h"
#include "fx/CyberFxDirector.h"
#include "fx/CyberBackdropComponent.h"
#include "fx/BootSequenceOverlay.h"


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
    std::unique_ptr<juce::ImageComponent> nkLogoView;
    std::unique_ptr<juce::Label>         pluginNameLabel;
    std::unique_ptr<juce::Label>         statusBarLabel; // live SR / OS / MIX / LIM
    std::unique_ptr<juce::TextButton>    helpButton;
    std::unique_ptr<juce::TextButton>    fxButton;
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
    std::unique_ptr<juce::Slider>        inputGainSlider;
    std::unique_ptr<juce::Slider>        mixSlider;
    std::unique_ptr<juce::Label>         inputGainLabel;
    std::unique_ptr<juce::Label>         mixLabel;
    std::unique_ptr<juce::Label>         inputGainValue;
    std::unique_ptr<juce::Label>         mixValue;
    std::unique_ptr<juce::Label>         currentPresetLabel;
    std::unique_ptr<juce::ToggleButton>  inputLeftButton;
    std::unique_ptr<juce::ToggleButton>  inputRightButton;
    std::unique_ptr<juce::ComboBox>      polisherBox;
    std::unique_ptr<juce::Label>         polisherLabel;
    std::unique_ptr<juce::ComboBox>      oversamplingBox;
    std::unique_ptr<juce::Label>         oversamplingLabel;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
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
    std::unique_ptr<BootSequenceOverlay>    bootOverlay;
    bool bootPlayed { false };
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



