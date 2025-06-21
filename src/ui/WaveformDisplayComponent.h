#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

class WaveformDisplayComponent : public juce::Component, private juce::Timer
{
public:
    enum class Type { Input, Output };

    WaveformDisplayComponent(NeuroCoreAudioProcessor& proc, Type t);
    ~WaveformDisplayComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override;

    NeuroCoreAudioProcessor& processor;
    Type type;
    juce::AudioBuffer<float> buffer {1, Config::kWaveformDisplaySamples};
};

