#pragma once

#include <JuceHeader.h>

class CyberBackdropCache
{
public:
    void ensure (int w, int h);
    void draw (juce::Graphics& g, float hexScrollPx) const;
    int  rebuildCount() const noexcept { return rebuilds; }

private:
    void rebuild();

    juce::Image cache;
    juce::Image hexStrip;
    int width { 0 };
    int height { 0 };
    int rebuilds { 0 };
};
