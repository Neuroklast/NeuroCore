#pragma once

#include <JuceHeader.h>

/** Shared cinematic chrome for overlay / boot. Message-thread only. */
namespace CyberChrome
{
    void drawScanlines (juce::Graphics& g, juce::Rectangle<int> r, float timeSec, float alpha);
    void drawVignette (juce::Graphics& g, juce::Rectangle<int> r, float alpha);
    /** Soft phosphor glass: faint scan + vignette. Full motion only. */
    void drawCrtGlow (juce::Graphics& g, juce::Rectangle<int> r, float timeSec, float peak01);
    void drawChromaticInset (juce::Graphics& g, juce::Rectangle<int> r, float amount);
    void drawGlitchSlices (juce::Graphics& g, juce::Rectangle<int> r, float amount, int seed, int count);
    void drawScanBeam (juce::Graphics& g, juce::Rectangle<int> r, float y01, float alpha);
    void drawHudCorners (juce::Graphics& g, juce::Rectangle<float> r, float reveal01, float thick = 2.f);
    void drawBlockBar (juce::Graphics& g, juce::Rectangle<int> r, float progress01);
    void drawNoise (juce::Graphics& g, juce::Rectangle<int> r, float alpha, int seed);
    void drawHexMeta (juce::Graphics& g, juce::Rectangle<int> r, float progress01);

    juce::String statusForProgress (float progress01);
    juce::String loaderLabelForClip (int clipTypeIndex);
    juce::String loadingTextAt (float tSec, bool teardown);

    /** Interior of the assembling modal: bar, hex, console, checksum. */
    void drawOverlayLoader (juce::Graphics& g,
                            juce::Rectangle<int> inner,
                            float tSec,
                            float bar01,
                            int clipTypeIndex,
                            int seed,
                            bool teardown);
}
