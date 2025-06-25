#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "CircularProgressBar.h"

class ProgressOverlay : public juce::Component
{
public:
    ProgressOverlay(const juce::String& message, std::atomic<float>& progress);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String text;
    std::atomic<float>& progressValue;
    CircularProgressBar progressBar;
    juce::Rectangle<int> messageArea;
};
