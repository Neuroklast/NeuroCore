#include "CyberFxDirector.h"
#include <cmath>

CyberFxDirector::CyberFxDirector (CyberFxConfig cfg)
    : config (cfg)
{
}

void CyberFxDirector::setVisible (bool shouldBeVisible)
{
    state.visible = shouldBeVisible;
    if (! shouldBeVisible)
        state.glitch = 0.f;
}

void CyberFxDirector::setMotion (CyberMotion motion)
{
    state.motion = motion;
    if (motion == CyberMotion::Reduced)
        state.glitch = 0.f;
}

void CyberFxDirector::setPeakNorm (float norm01)
{
    peakTarget = juce::jlimit (0.f, 1.f, norm01);
}

void CyberFxDirector::triggerGlitch (float strength01, int seed)
{
    if (! state.visible || state.motion == CyberMotion::Reduced)
        return;

    state.glitch = juce::jlimit (0.f, 1.f, strength01);
    state.glitchSeed = seed;
}

void CyberFxDirector::tick (float dtSec, juce::Random& rng)
{
    const float dt = juce::jlimit (0.f, 0.05f, dtSec);
    state.timeSec += dt;
    state.peakPulse += (peakTarget - state.peakPulse) * config.pulseSmooth;

    if (! state.visible || state.motion == CyberMotion::Reduced)
    {
        state.glitch = 0.f;
        return;
    }

    glitchGap -= dt;
    if (glitchGap <= 0.f)
    {
        glitchGap = config.glitchMinGapSec
                  + rng.nextFloat() * (config.glitchMaxGapSec - config.glitchMinGapSec);
        if (rng.nextFloat() < config.glitchChance)
            triggerGlitch (config.glitchMin + rng.nextFloat() * (config.glitchMax - config.glitchMin),
                           rng.nextInt());
    }

    state.glitch *= 0.7f;
    if (state.glitch < 0.01f)
        state.glitch = 0.f;

    const bool glitchOn = state.glitch > kGlitchPaintMin;
    const float beamRow = std::floor (std::fmod (state.timeSec * 90.f, 2000.f));
    const float hexRow  = std::floor (state.timeSec * 40.f);
    ambientDirty = glitchOn || glitchOn != lastGlitchPaint
                || beamRow != lastBeamRow || hexRow != lastHexRow;
    lastGlitchPaint = glitchOn;
    lastBeamRow = beamRow;
    lastHexRow = hexRow;
}

bool CyberFxDirector::needsAmbientRepaint() const noexcept
{
    return ambientDirty;
}
