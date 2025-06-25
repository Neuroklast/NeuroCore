#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

class NeuroCoreAudioProcessor;

// modal overlay showing preset table with action buttons
class PresetOverlay : public juce::Component
{
public:
    PresetOverlay(NeuroCoreAudioProcessor& processor, juce::LookAndFeel& lf);
    ~PresetOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;
    void mouseUp(const juce::MouseEvent& ev) override;

    std::function<void(int)> onPresetSelected; //!< callback when user chooses a preset
    std::function<void()> onClose;             //!< called when overlay should close

private:
    PresetTableComponent table;
    NeuroCoreAudioProcessor& processor;
    juce::LookAndFeel& lookAndFeel;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton closeButton { "Close" };
    juce::Rectangle<int> panel;

    void refreshTable();
};

class ModalCallback : public juce::ModalComponentManager::Callback
{
public:
    ModalCallback(std::function<void(int)> callback) : callback(std::move(callback)) {}

    void modalStateFinished(int result) override
    {
        if (callback)
            callback(result);
    }

private:
    std::function<void(int)> callback;
};