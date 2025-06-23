#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"
#include "WaveformLookAndFeel.h"

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
    void setWaveColour(juce::Colour c) noexcept { waveColour = c; }
    void setLineWidth(float w) noexcept { lineWidth = w; }
    void enableEcho(bool e) noexcept { showEcho = e; }

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
    juce::Colour waveColour { juce::Colour(Config::kWaveformColourARGB) };
    float lineWidth { Config::kWaveformLineWidth };
    bool showEcho { false };
    WaveformLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this };
    std::deque<std::vector<float>> history;

    std::vector<float> fftMagnitudes;
    static constexpr int fftOrder = 11; // 2048 point FFT
};
