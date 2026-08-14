#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

/** Vertical loudness bar — same angular hull / accent gradient as the Mix slider. */
class LoudnessMeterComponent : public juce::Component,
                               private juce::Timer
{
public:
    explicit LoudnessMeterComponent(NeuroCoreAudioProcessor& proc);
    ~LoudnessMeterComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent& e) override;
    /** Hide while a modal covers the editor (OpenGL used to sit above siblings). */
    void setCoveredByOverlay (bool covered);

    /** 0 at idle / low level, 1 at full scale. Cubic so the bottom stays calm. */
    static float glitchAmount (float fillNorm) noexcept;
    /** Band height in px: quiet = 2, high + near the top of the bar = chunky. */
    static int bandHeightPx (float fillNorm, float heightFromBottom01) noexcept;

private:
    void timerCallback() override;
    void drawLed(juce::Graphics& g, juce::Rectangle<float> area, bool on);
    juce::Path makeHull (juce::Rectangle<float> r) const;
    void drawOverloadFill (juce::Graphics& g,
                           juce::Rectangle<float> meterArea,
                           float fillTop,
                           float fillNorm) noexcept;

    juce::Random rng;
    int glitchSeed { 1 };

    enum class Scale { dBFS, LUFS, KSystem };
    struct ScaleInfo { const char* name; float minDb; float maxDb; float step; };
    ScaleInfo currentScaleInfo() const noexcept;
    float valueToY(float db, juce::Rectangle<float> area) const noexcept;
    void showContextMenu();

    NeuroCoreAudioProcessor& processor;
    float loudness { -100.0f };
    bool  limiter { false };
    bool  blink   { false };
    int   blinkCount { 0 };

    Scale scale { Scale::dBFS };
    bool coveredByOverlay { false };
};
