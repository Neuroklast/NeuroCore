#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

class LoudnessMeterComponent : public juce::Component, private juce::Timer
{
public:
    explicit LoudnessMeterComponent(NeuroCoreAudioProcessor& proc);
    ~LoudnessMeterComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override;
    void drawLed(juce::Graphics& g, juce::Rectangle<float> area, bool on);

    NeuroCoreAudioProcessor& processor;
    float loudness { -100.0f };
    bool  limiter { false };
    bool  blink   { false };
    int   blinkCount { 0 };
};

