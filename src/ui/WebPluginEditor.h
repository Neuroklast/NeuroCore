#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

/** Thin frame around the processor-owned WebView. Close does not destroy the browser. */
class WebPluginEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget
{
public:
    explicit WebPluginEditor (NeuroKoreAudioProcessor&);
    ~WebPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    float getDesktopScaleFactor() const override { return 1.0f; }
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    NeuroKoreAudioProcessor& audioProcessor;
    juce::ComponentBoundsConstrainer sizeConstrain;
    std::unique_ptr<juce::ResizableCornerComponent> cornerGrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebPluginEditor)
};

juce::AudioProcessorEditor* createWebEditor (NeuroKoreAudioProcessor&);
