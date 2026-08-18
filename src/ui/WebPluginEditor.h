#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../bridge/WebBridge.h"
#include "../bridge/WebAssets.h"
#include "../bridge/CompileSession.h"

class WebPluginEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit WebPluginEditor (NeuroKoreAudioProcessor&);
    ~WebPluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    float getDesktopScaleFactor() const override { return 1.0f; }

private:
    struct PageBrowser : juce::WebBrowserComponent
    {
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override;
        juce::String allowedDevUrl;
    };

    std::optional<juce::WebBrowserComponent::Resource> provideResource (const juce::String& url);
    juce::WebBrowserComponent::Options makeOptions();

    void pushOutcome (const bridge::CompileOutcome& out);
    void pushHost();
    void timerCallback() override;
    void pickFile (const juce::String& kind, const juce::String& slot);
    void irSlot (const juce::String& action, const juce::String& slot);

    NeuroKoreAudioProcessor& audioProcessor;
    bridge::WebBridge bridge;
    bridge::CompileSession session;
    juce::File distRoot;
    juce::ComponentBoundsConstrainer sizeConstrain;
    std::unique_ptr<PageBrowser> browser;
    std::unique_ptr<juce::ResizableCornerComponent> cornerGrip;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebPluginEditor)
};

juce::AudioProcessorEditor* createWebEditor (NeuroKoreAudioProcessor&);
