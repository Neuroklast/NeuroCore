#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

//==============================================================================
// Component that draws a sine wave and its transformation through the active
// formula. Provides a zoom slider for horizontal scaling.
class FormulaWaveComponent : public juce::Component, private juce::Timer
{
public:
    explicit FormulaWaveComponent (NeuroCoreAudioProcessor& p);
    ~FormulaWaveComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateWave();

    NeuroCoreAudioProcessor& processor;
    juce::Slider            zoomSlider;
    float                   zoom { 1.0f };

    static constexpr int    numSamples = 512;
    std::array<float, numSamples> inputValues {};
    std::array<float, numSamples> outputValues {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormulaWaveComponent)
};
