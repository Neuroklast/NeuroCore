#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"
#include "custom/ModalOverlay.h"

class NeuroCoreAudioProcessor;

// modal overlay showing preset table with action buttons
class PresetOverlay : public ui::ModalOverlay
{
public:
    PresetOverlay(NeuroCoreAudioProcessor& processor, juce::LookAndFeel& lf);
    ~PresetOverlay() override;

    void resized() override;

    std::function<void(int)> onPresetSelected; //!< callback when user chooses a preset

private:
    PresetTableComponent table;
    NeuroCoreAudioProcessor& processor;
    juce::LookAndFeel& lookAndFeel;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton closeButton { "Close" };

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