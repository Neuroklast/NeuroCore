#pragma once

#include <JuceHeader.h>
#include "CyberFxTypes.h"

class CyberFxDirector
{
public:
    explicit CyberFxDirector (CyberFxConfig cfg = {});

    void setVisible (bool shouldBeVisible);
    void setMotion (CyberMotion motion);
    void setPeakNorm (float norm01);
    void triggerGlitch (float strength01, int seed);
    void tick (float dtSec, juce::Random& rng);

    const CyberFxState& getState() const noexcept { return state; }
    bool needsAmbientRepaint() const noexcept;

private:
    CyberFxConfig config;
    CyberFxState state;
    float peakTarget { 0.f };
    float glitchGap { 2.5f };
    float lastBeamRow { -1.f };
    float lastHexRow { -1.f };
    bool  lastGlitchPaint { false };
    bool  ambientDirty { true };
};
