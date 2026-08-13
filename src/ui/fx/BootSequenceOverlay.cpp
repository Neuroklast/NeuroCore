#include "BootSequenceOverlay.h"
#include "DecodeText.h"
#include "../PluginLookAndFeel.h"

BootSequenceOverlay::BootSequenceOverlay()
{
    setOpaque (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);
}

void BootSequenceOverlay::startOn (juce::Component& parent)
{
    parent.addAndMakeVisible (this);
    setBounds (parent.getLocalBounds());
    toFront (true);
    grabKeyboardFocus();
    elapsed = 0.f;
    finished = false;
    lastStamp = 0.0;
    sequence.playEnter();
    vblank = juce::VBlankAttachment { this, [this] (double now) { onVBlank (now); } };
}

void BootSequenceOverlay::skip()
{
    sequence.skipToEnd();
    finish();
}

void BootSequenceOverlay::onVBlank (double nowSec)
{
    if (finished)
        return;

    if (lastStamp <= 0.0)
        lastStamp = nowSec;

    const float dt = juce::jlimit (0.f, 0.05f, (float) (nowSec - lastStamp));
    lastStamp = nowSec;
    elapsed += dt;
    sequence.tick (dt);
    repaint();

    if (elapsed >= kBootMaxSec)
        finish();
}

void BootSequenceOverlay::finish()
{
    if (finished)
        return;
    finished = true;
    vblank = {};
    setVisible (false);
    setInterceptsMouseClicks (false, false);
    if (onFinished)
        onFinished();
}

void BootSequenceOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (juce::jmax (sequence.scrimAlpha(), 0.88f)));

    const auto slice = sequence.sliceAmount();
    if (slice > 0.05f)
    {
        const int y = (int) (sequence.wipeY01() * (float) juce::jmax (1, getHeight() - 20));
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.18f * slice));
        g.fillRect (0, y, getWidth(), 10);
        g.setColour (juce::Colour (0x44ff0044));
        g.fillRect (4, y + 2, getWidth(), 2);
        g.setColour (juce::Colour (0x4400ffff));
        g.fillRect (-3, y + 6, getWidth(), 2);
    }

    const float revealT = juce::jlimit (0.f, 1.f, elapsed / 0.45f);
    const char* lines[] = {
        "NEUROCORE // NETRUNNER OS",
        "> LINK",
        "> DSP CORE",
        "> ACCESS"
    };

    g.setFont (NeuroCoreLookAndFeel::monoFont (18.f));
    auto area = getLocalBounds().reduced (40).withSizeKeepingCentre (juce::jmin (720, getWidth() - 80), 160);
    for (int i = 0; i < 4; ++i)
    {
        const int revealed = (int) std::round (revealT * (float) juce::String (lines[i]).length());
        const auto text = decodeGlitchText (lines[i], revealed, rng);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f + 0.45f * sequence.contentAlpha()));
        g.drawText (text, area.removeFromTop (32), juce::Justification::centredLeft, false);
    }
}

void BootSequenceOverlay::resized()
{
}

void BootSequenceOverlay::mouseUp (const juce::MouseEvent&)
{
    skip();
}

bool BootSequenceOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey
        || key == juce::KeyPress::spaceKey)
    {
        skip();
        return true;
    }
    return false;
}
