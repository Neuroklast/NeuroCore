#pragma once
#include <JuceHeader.h>

class NeuroKoreAudioProcessor;

/** Modal optimizer: shows report, original vs optimized, Apply / Cancel. */
class OptimizeContentComponent : public juce::Component
{
public:
    OptimizeContentComponent (NeuroKoreAudioProcessor& proc,
                              const juce::String& sourceScript);

    std::function<void(const juce::String& optimized)> onApply;
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void runOptimize();

    NeuroKoreAudioProcessor& processor;
    juce::String original;
    juce::String optimized;

    juce::Label titleLabel, summaryLabel;
    juce::TextEditor beforeEditor, afterEditor, logEditor;
    juce::TextButton applyButton { "Apply" };
    juce::TextButton reRunButton { "Re-run" };
    juce::TextButton closeButton { "Close" };
    juce::Label beforeLabel, afterLabel, logLabel;
};
