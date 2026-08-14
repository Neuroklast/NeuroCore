#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "WaveformDisplayComponent.h"
#include "ScopeAnalytics.h"

/** Slim L/R peak+RMS bars for one scope (IN or OUT). Same height as the wave. */
class IoLoudnessMeter : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    IoLoudnessMeter (NeuroCoreAudioProcessor& proc, WaveformDisplayComponent::Type t);
    ~IoLoudnessMeter() override;

    void paint (juce::Graphics& g) override;
    void resized() override {}

    float displayedRmsDbL() const noexcept { return rmsDbL; }
    float displayedRmsDbR() const noexcept { return rmsDbR; }

private:
    void timerCallback() override;
    void drawBar (juce::Graphics& g, juce::Rectangle<float> r,
                  float rmsDb, float peakDb, const char* tag) const;

    NeuroCoreAudioProcessor& processor;
    WaveformDisplayComponent::Type type;
    juce::AudioBuffer<float> buffer { Config::kMaxChannels, Config::kWaveformDisplaySamples };
    float rmsDbL { -100.f }, rmsDbR { -100.f };
    float peakDbL { -100.f }, peakDbR { -100.f };
};
