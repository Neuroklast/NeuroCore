#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include <vector>

/**
    Software-rendered signal scope (no OpenGL — host-safe, always visible).
    Draws with multi-pass glow for cyber HUD quality.
*/
class WaveformDisplayComponent : public juce::Component,
                                 public juce::SettableTooltipClient,
                                 private juce::Timer
{
public:
    enum class Type { Input, Output };
    enum class XScale { Samples, Time, Frequency };
    enum class YScale { Linear, Decibel };

    WaveformDisplayComponent (NeuroCoreAudioProcessor& proc, Type t);
    ~WaveformDisplayComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override {}
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;

    void setXScale (XScale s) noexcept { xScale = s; }
    void setYScale (YScale s) noexcept { yScale = s; }
    void setZoom (float z) noexcept { zoom = juce::jlimit (1.0f, 10.0f, z); }
    void setFixedWave (bool f) noexcept { fixedWave = f; }

    float lineThickness { 1.6f };
    juce::Colour lineColour { juce::Colour (0xffff1a1a) };

private:
    void timerCallback() override;
    void drawScope (juce::Graphics& g, juce::Rectangle<float> plot);
    void drawAxes (juce::Graphics& g, juce::Rectangle<float> plot);
    float valueToY (float v, juce::Rectangle<float> area) const;
    float indexToX (int i, int total, juce::Rectangle<float> area) const;
    void updateTooltip (juce::Point<int> pos, juce::Rectangle<float> area);
    juce::Rectangle<float> plotArea() const;

    NeuroCoreAudioProcessor& processor;
    Type type;
    /** Stereo capture; display uses L+R mono mix for fair before/after compare. */
    juce::AudioBuffer<float> buffer { Config::kMaxChannels, Config::kWaveformDisplaySamples };
    /** Scratch mono line used for scope + FFT (aligned in/out). */
    std::vector<float> monoScratch;

    XScale xScale { XScale::Samples };
    YScale yScale { YScale::Linear };
    bool showGrid { true };
    bool invertY { false };
    /**
        If true, IN finds a rising zero-cross and both IN/OUT use that same offset
        so the two scopes are time-aligned (true before/after). OUT never picks its
        own zero — that was desyncing wet vs dry after distortion.
    */
    bool fixedWave { true };
    float zoom { 1.0f };
    juce::TooltipWindow tooltipWindow { this };

    /** Display samples (snapshotted, not SmoothedValue — avoids drain/jitter) */
    std::vector<float> displayData;
    std::vector<float> fftMagnitudes;
    static constexpr int fftOrder = Config::kWaveformFftOrder;

    void fillMonoFromBuffer (std::vector<float>& dest) const;
};
