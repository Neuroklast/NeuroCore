#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"
class NeuroCoreAudioProcessor;

class PresetContentComponent : public juce::Component
{
public:
    PresetContentComponent(NeuroCoreAudioProcessor& processor, juce::LookAndFeel& lf);
    ~PresetContentComponent() override;

    std::function<void(int)> onPresetSelected;
    std::function<void()> onClose;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseUp(const juce::MouseEvent& ev) override;

private:
    PresetTableComponent table;
    NeuroCoreAudioProcessor& processor;
    juce::LookAndFeel& lookAndFeel;
    juce::TextButton loadButton{"Load"};
    juce::TextButton saveButton{"Save"};
    juce::TextButton deleteButton{"Delete"};
    juce::TextButton closeButton{"Close"};
    juce::Rectangle<int> panel;

    void refreshTable();
};
