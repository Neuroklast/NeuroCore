#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

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

    static constexpr int    numSamples = Config::kFormulaPreviewSamples;
    std::array<float, numSamples> inputValues {};
    std::array<float, numSamples> outputValues {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormulaWaveComponent)
};
