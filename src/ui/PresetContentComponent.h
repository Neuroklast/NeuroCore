#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

class NeuroCoreAudioProcessor;

// component showing preset table with action buttons
class PresetContentComponent : public juce::Component
{
public:
    PresetContentComponent(NeuroCoreAudioProcessor& processor, juce::LookAndFeel& lf);
    ~PresetContentComponent() override;

    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;

    std::function<void(int)> onPresetSelected;
    std::function<void()> onClose;

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
    explicit ModalCallback(std::function<void(int)> cb) : callback(std::move(cb)) {}
    void modalStateFinished(int result) override
    {
        if (callback)
            callback(result);
    }
private:
    std::function<void(int)> callback;
};
