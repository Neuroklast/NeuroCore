#pragma once

#include <JuceHeader.h>

/** Shared cinematic chrome for overlay / boot. Message-thread only. */
namespace CyberChrome
{
    void drawScanlines (juce::Graphics& g, juce::Rectangle<int> r, float timeSec, float alpha);
    void drawVignette (juce::Graphics& g, juce::Rectangle<int> r, float alpha);
    void drawChromaticInset (juce::Graphics& g, juce::Rectangle<int> r, float amount);
    void drawGlitchSlices (juce::Graphics& g, juce::Rectangle<int> r, float amount, int seed, int count);
    void drawScanBeam (juce::Graphics& g, juce::Rectangle<int> r, float y01, float alpha);
    void drawHudCorners (juce::Graphics& g, juce::Rectangle<float> r, float reveal01, float thick = 2.f);
    void drawBlockBar (juce::Graphics& g, juce::Rectangle<int> r, float progress01);
    void drawNoise (juce::Graphics& g, juce::Rectangle<int> r, float alpha, int seed);
    void drawHexMeta (juce::Graphics& g, juce::Rectangle<int> r, float progress01);

    juce::String statusForProgress (float progress01);
}
