#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

class IrPanelComponent : public juce::Component,
                         public juce::FileDragAndDropTarget
{
public:
    IrPanelComponent (NeuroKoreAudioProcessor& proc, juce::String slot);
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void drawWave (juce::Graphics& g, juce::Rectangle<int> area);
    NeuroKoreAudioProcessor& processor;
    juce::String slotId;
    juce::TextButton loadButton { "Load" };
    juce::TextButton playButton { "Play" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton closeButton { "Close" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> fileChooser;
};
