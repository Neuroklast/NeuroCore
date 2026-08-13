#include "CyberBackdropComponent.h"
#include "../PluginLookAndFeel.h"
#include <cmath>

CyberBackdropComponent::CyberBackdropComponent (CyberFxDirector& directorIn)
    : director (directorIn)
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
}

void CyberBackdropComponent::setPeakFromDb (float loudnessDb)
{
    const float norm = std::isfinite (loudnessDb)
                         ? juce::jlimit (0.f, 1.f, (loudnessDb + 40.f) / 40.f)
                         : 0.f;
    director.setPeakNorm (norm);
}

void CyberBackdropComponent::advance (float dtSec)
{
    director.tick (dtSec, rng);
    if (director.needsAmbientRepaint())
        repaint();
}

void CyberBackdropComponent::resized()
{
    cache.ensure (getWidth(), getHeight());
}

void CyberBackdropComponent::paint (juce::Graphics& g)
{
    cache.ensure (getWidth(), getHeight());
    const auto& s = director.getState();
    cache.draw (g, s.timeSec * 40.f);

    const float h = (float) getHeight();
    const float w = (float) getWidth();
    const float beamY = std::fmod (s.timeSec * 90.f, h + 40.f) - 20.f;
    juce::ColourGradient beam (juce::Colours::transparentBlack, 0, beamY - 12.f,
                               NeuroCoreLookAndFeel::accent().withAlpha (0.12f), 0, beamY, false);
    g.setGradientFill (beam);
    g.fillRect (0.f, beamY - 12.f, w, 24.f);

    if (s.glitch > kGlitchPaintMin)
    {
        const float a = s.glitch;
        const int sliceY = (std::abs (s.glitchSeed) % juce::jmax (1, getHeight() - 40)) + 20;
        const int sliceH = 8 + (std::abs (s.glitchSeed) % 28);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.08f * a));
        g.fillRect (0, sliceY, getWidth(), sliceH);
        g.setColour (juce::Colour (0x44ff0044));
        g.fillRect ((int) (a * 6.f), sliceY + 2, getWidth(), 2);
        g.setColour (juce::Colour (0x4400ffff));
        g.fillRect ((int) (-a * 5.f), sliceY + 5, getWidth(), 2);
    }

    g.setColour (juce::Colour (0xff0a0000));
    g.fillRect (0, 0, getWidth(), 22);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.9f));
    g.fillRect (0, 0, getWidth(), 2);
    g.setFont (NeuroCoreLookAndFeel::monoFont (11.f));
    g.setColour (NeuroCoreLookAndFeel::accent());
    const bool blink = ((int) (s.timeSec * 2.f) % 2) == 0;
    juce::String hdr = "NEUROCORE  //  NETRUNNER OS  //  LINK ";
    hdr << (blink ? "ACTIVE" : "active");
    g.drawText (hdr, 10, 4, getWidth() - 20, 16, juce::Justification::centredLeft, false);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f));
    g.drawText (juce::String::formatted ("T+%.1f", s.timeSec),
                10, 4, getWidth() - 20, 16, juce::Justification::centredRight, false);
}
