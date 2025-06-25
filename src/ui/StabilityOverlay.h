#pragma once
#include <JuceHeader.h>

class StabilityOverlay : public juce::Component
{
public:
    explicit StabilityOverlay(const juce::String& message);
    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onDismiss;

private:
    void exit();
    juce::String text;
    juce::TextButton okButton { "OK" };
    juce::Rectangle<int> messageArea;
};
