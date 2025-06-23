#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

class WaveformDisplayComponent : public juce::Component,
                                 public juce::SettableTooltipClient,
                                 private juce::OpenGLRenderer,
                                 private juce::Timer
{
public:
    enum class Type { Input, Output };
    enum class XScale { Samples, Time, Frequency };
    enum class YScale { Linear, Decibel };

    WaveformDisplayComponent(NeuroCoreAudioProcessor& proc, Type t);
    ~WaveformDisplayComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setXScale(XScale s) noexcept { xScale = s; }
    void setYScale(YScale s) noexcept { yScale = s; }
    void setZoom(float z) noexcept { zoom = juce::jlimit(1.0f, 10.0f, z); }
    void setFixedWave(bool f) noexcept { fixedWave = f; }

    float lineThickness { 1.5f };
    juce::Colour lineColour { juce::Colours::cyan };

private:
    void timerCallback() override;
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override {}

    void drawAxes(juce::Graphics& g);
    float valueToY(float v, juce::Rectangle<float> area) const;
    float indexToX(int i, int total, juce::Rectangle<float> area) const;
    void updateTooltip(juce::Point<int> pos, juce::Rectangle<float> area);

    NeuroCoreAudioProcessor& processor;
    Type type;
    juce::AudioBuffer<float> buffer {1, Config::kWaveformDisplaySamples};
    juce::OpenGLContext openGLContext;

    XScale xScale { XScale::Samples };
    YScale yScale { YScale::Linear };
    bool showGrid { true };
    bool invertY { false };
    bool fixedWave { true };
    float zoom { 1.0f };
    juce::TooltipWindow tooltipWindow { this };

    std::vector<juce::SmoothedValue<float>> smoothedData;
    std::vector<juce::SmoothedValue<float>> smoothedFft;
    std::vector<float> fftMagnitudes;
    static constexpr int fftOrder = 11; // 2048 point FFT
};
