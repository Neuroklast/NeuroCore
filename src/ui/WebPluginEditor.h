#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

/** Thin frame around the processor-owned WebView. Close does not destroy the browser. */
class WebPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit WebPluginEditor (NeuroKoreAudioProcessor&);
    ~WebPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    float getDesktopScaleFactor() const override { return 1.0f; }

private:
    NeuroKoreAudioProcessor& audioProcessor;
    juce::ComponentBoundsConstrainer sizeConstrain;
    std::unique_ptr<juce::ResizableCornerComponent> cornerGrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebPluginEditor)
};

juce::AudioProcessorEditor* createWebEditor (NeuroKoreAudioProcessor&);
