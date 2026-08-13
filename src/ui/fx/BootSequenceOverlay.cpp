#include "BootSequenceOverlay.h"
#include "DecodeText.h"
#include "CyberChrome.h"
#include "../PluginLookAndFeel.h"
#include "../../core/Config.h"

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
    const auto bounds = getLocalBounds();
    const float p = juce::jlimit (0.f, 1.f, elapsed / kBootMaxSec);
    const float slice = juce::jmax (sequence.sliceAmount(), p < 0.92f ? 0.35f : 0.08f);

    g.fillAll (juce::Colours::black);
    CyberChrome::drawVignette (g, bounds, 0.85f);
    CyberChrome::drawScanlines (g, bounds, elapsed * 10.f, 0.75f);
    CyberChrome::drawNoise (g, bounds, 0.12f + 0.18f * slice, rng.nextInt());
    CyberChrome::drawChromaticInset (g, bounds, 0.4f + 0.6f * (1.f - p));
    CyberChrome::drawGlitchSlices (g, bounds, slice, rng.nextInt(), 5);
    CyberChrome::drawScanBeam (g, bounds, std::fmod (elapsed * 0.85f, 1.f), 0.7f);

    auto frame = bounds.withSizeKeepingCentre (juce::jmin (640, getWidth() - 48), 280).toFloat();
    CyberChrome::drawHudCorners (g, frame, juce::jmin (1.f, p * 1.6f), 2.4f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.22f));
    g.drawRect (frame, 1.f);

    auto area = frame.reduced (28.f, 22.f).toNearestInt();
    const juce::String title = Config::kOsBanner;
    const int revealed = (int) std::round (juce::jlimit (0.f, 1.f, p / 0.45f) * (float) title.length());
    g.setFont (NeuroCoreLookAndFeel::monoFont (22.f));
    g.setColour (NeuroCoreLookAndFeel::accent());
    g.drawText (decodeGlitchText (title, revealed, rng),
                area.removeFromTop (32), juce::Justification::centredLeft, false);

    g.setFont (NeuroCoreLookAndFeel::monoFont (11.f));
    g.setColour (NeuroCoreLookAndFeel::mutedText());
    g.drawText ("// BOOT SEQUENCE", area.removeFromTop (18), juce::Justification::centredLeft, false);

    CyberChrome::drawBlockBar (g, area.removeFromTop (22), p);
    area.removeFromTop (10);

    g.setFont (NeuroCoreLookAndFeel::monoFont (13.f));
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.85f));
    g.drawText (CyberChrome::statusForProgress (p),
                area.removeFromTop (20), juce::Justification::centredLeft, false);

    area.removeFromTop (8);
    CyberChrome::drawHexMeta (g, area.removeFromTop (16), p);

    g.setFont (NeuroCoreLookAndFeel::monoFont (10.f));
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.4f));
    g.drawText ("ESC SKIP", area.removeFromBottom (16), juce::Justification::centredRight, false);
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
