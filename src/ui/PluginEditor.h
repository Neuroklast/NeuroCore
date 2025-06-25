/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include "PluginLookAndFeel.h"
#include "../utils/FormulaHelper.h"
#include "FormulaDisplayComponent.h"
#include "WaveformDisplayComponent.h"
#include "InlineAutocompleteEditor.h"
#include "LoudnessMeterComponent.h"
#include "StabilityOverlay.h"
#include "custom/ParameterComponent.h"
#include "../utils/Localiser.h"
#include "PresetTableComponent.h"
#include "WeightedLayout.h"
#if __has_include(<melatonin_inspector/melatonin_inspector.h>)
# include <melatonin_inspector/melatonin_inspector.h>
#endif

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
                                       private Localiser::Listener
{
public:
    NeuroCoreAudioProcessorEditor (NeuroCoreAudioProcessor&);
    ~NeuroCoreAudioProcessorEditor() override;

    juce::String getFormulaText() const;
    void setFormulaText(const juce::String& text);
    void showStabilityWarning(const juce::String& msg);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;



private:
    void refreshParameterControls();
    /// Updates all text labels after language change.
    void updateTranslations();
    void languageChanged() override { updateTranslations(); }
    NeuroCoreAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::unique_ptr<ui::LayoutNode> layoutRoot;

    // Left column controls

    std::array<std::unique_ptr<ui::ParameterComponent>, 4> paramComponents;
    std::array<std::unique_ptr<juce::TextEditor>, 4>          nameEditors;
    std::unique_ptr<juce::Label>         pluginNameLabel;
    std::unique_ptr<juce::TextButton>    helpButton;
    std::unique_ptr<juce::ToggleButton>  blankToggle;
    std::unique_ptr<juce::TextButton>    presetsButton;
    std::unique_ptr<juce::ToggleButton>  bypassButton;
    std::unique_ptr<juce::TextButton>    functionsButton;
    std::unique_ptr<juce::TextButton>    stagesButton;
    std::unique_ptr<juce::Slider>        inputGainSlider;
    std::unique_ptr<juce::Slider>        mixSlider;
    std::unique_ptr<juce::Slider>        outputGainSlider;
    std::unique_ptr<juce::Label>         inputGainLabel;
    std::unique_ptr<juce::Label>         mixLabel;
    std::unique_ptr<juce::Label>         outputGainLabel;
    std::unique_ptr<juce::Label>         inputGainValue;
    std::unique_ptr<juce::Label>         mixValue;
    std::unique_ptr<juce::Label>         outputGainValue;
    std::unique_ptr<juce::ToggleButton>  inputLeftButton;
    std::unique_ptr<juce::ToggleButton>  inputRightButton;
    std::unique_ptr<juce::ComboBox>      languageBox;
    std::unique_ptr<juce::Label>         languageLabel;
    std::unique_ptr<juce::ComboBox>      polisherBox;
    std::unique_ptr<juce::Label>         polisherLabel;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>       polisherAttachment;

    // Middle column editors
    std::unique_ptr<InlineAutocompleteEditor> formulaInputEditor;
    std::unique_ptr<juce::TextButton>       optimizeButton;
    std::unique_ptr<juce::TextButton>       editSaveButton;
    std::unique_ptr<juce::Label>            errorLabel;
    bool                                    editing { false };

    NeuroCoreLookAndFeel lookAndFeel;
    std::unique_ptr<WaveformDisplayComponent> inputDisplay;
    std::unique_ptr<WaveformDisplayComponent> outputDisplay;
    std::unique_ptr<LoudnessMeterComponent>   loudnessMeter;
    std::unique_ptr<PresetTableComponent>     presetTable;
    std::unique_ptr<juce::DialogWindow>       presetWindow;
    std::unique_ptr<StabilityOverlay>         stabilityOverlay;

    //melatonin::Inspector inspector{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessorEditor)
};



