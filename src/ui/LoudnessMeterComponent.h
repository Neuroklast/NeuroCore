#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

class LoudnessMeterComponent : public juce::Component,
                               private juce::OpenGLRenderer,
                               private juce::Timer
{
public:
    explicit LoudnessMeterComponent(NeuroCoreAudioProcessor& proc);
    ~LoudnessMeterComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void drawLed(juce::Graphics& g, juce::Rectangle<float> area, bool on);
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override {}

    enum class Scale { dBFS, LUFS, KSystem };
    struct ScaleInfo { const char* name; float minDb; float maxDb; float step; };
    ScaleInfo currentScaleInfo() const noexcept;
    float valueToY(float db, juce::Rectangle<float> area) const noexcept;
    void showContextMenu();

    NeuroCoreAudioProcessor& processor;
    float loudness { -100.0f };
    juce::SmoothedValue<float> smoothedLoudness;
    bool  limiter { false };
    bool  blink   { false };
    int   blinkCount { 0 };

    Scale scale { Scale::dBFS };
    juce::OpenGLContext openGLContext;
};

