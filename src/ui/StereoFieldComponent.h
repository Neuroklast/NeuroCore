#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "WaveformDisplayComponent.h"
#include "ScopeAnalytics.h"

/** Compact L/R goniometer + correlation bar. Same height as the IN/OUT scope. */
class StereoFieldComponent : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    StereoFieldComponent (NeuroKoreAudioProcessor& proc, WaveformDisplayComponent::Type t);
    ~StereoFieldComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override {}

    const ScopeAnalytics::StereoStats& lastStats() const noexcept { return stats; }

private:
    void timerCallback() override;

    NeuroKoreAudioProcessor& processor;
    WaveformDisplayComponent::Type type;
    juce::AudioBuffer<float> buffer { Config::kMaxChannels, Config::kWaveformDisplaySamples };
    ScopeAnalytics::StereoStats stats;
};
