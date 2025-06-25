#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

class NeuroCoreAudioProcessor;

// modal overlay showing preset table with action buttons
class PresetOverlay : public juce::Component
{
public:
    explicit PresetOverlay(NeuroCoreAudioProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(int)> onPresetSelected; //!< callback when user chooses a preset
    std::function<void()> onClose;             //!< called when overlay should close

private:
    PresetTableComponent table;
    NeuroCoreAudioProcessor& processor;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton closeButton { "Close" };

    void refreshTable();
};
