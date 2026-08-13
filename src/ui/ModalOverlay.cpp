#include "ModalOverlay.h"
#include "PluginLookAndFeel.h"
#include "fx/DecodeText.h"
#include "fx/CyberChrome.h"

ModalOverlay::ModalOverlay()
{
    setInterceptsMouseClicks (true, true);
    setOpaque (false);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (okButton);
    addAndMakeVisible (backButton);
    addAndMakeVisible (closeButton);

    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    titleLabel.setFont (NeuroCoreLookAndFeel::brandFont (15.0f, true));

    okButton.onClick = [this]
    {
        if (onAccept)
            onAccept();
    };
    backButton.onClick = [this] { requestClose(); };
    closeButton.onClick = [this] { requestClose(); };

    updateButtonVisibility();
}

ModalOverlay::~ModalOverlay()
{
    stopVBlank();
}

void ModalOverlay::setMode (OverlayMode newMode)
{
    mode = newMode;
    updateButtonVisibility();
}

void ModalOverlay::setMotion (CyberMotion newMotion)
{
    motion = newMotion;
}

void ModalOverlay::setContent (std::unique_ptr<juce::Component> newContent)
{
    if (content)
        removeChildComponent (content.get());
    content = std::move (newContent);
    if (content)
        addAndMakeVisible (*content);
    applyContentVisibility();
    resized();
}

void ModalOverlay::setTitle (const juce::String& text)
{
    rawTitle = text;
    titleLabel.setText (text, juce::dontSendNotification);
    resized();
}

void ModalOverlay::setPreferredContentSize (int w, int h)
{
    preferredW = w;
    preferredH = h;
    resized();
}

void ModalOverlay::show (juce::Component& parent)
{
    parent.addAndMakeVisible (this);
    setBounds (parent.getLocalBounds());
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    lastStamp = 0.0;
    sequence.playEnter();
    if (motion == CyberMotion::Reduced)
        sequence.skipToEnd();
    applyContentVisibility();
    resized();
    if (sequence.isBusy())
        startVBlank();
    else
        stopVBlank();
}

void ModalOverlay::requestClose()
{
    if (motion == CyberMotion::Reduced
        || sequence.getPhase() == OverlayPhase::Idle
        || sequence.getPhase() == OverlayPhase::Closed)
    {
        sequence.playExit();
        sequence.skipToEnd();
        finishIfClosed();
        return;
    }

    sequence.playExit();
    applyContentVisibility();
    startVBlank();
}

void ModalOverlay::skipToEnd()
{
    sequence.skipToEnd();
    applyContentVisibility();
    finishIfClosed();
}

void ModalOverlay::startVBlank()
{
    if (vblank.isEmpty())
        vblank = juce::VBlankAttachment { this, [this] (double now) { onVBlank (now); } };
}

void ModalOverlay::stopVBlank()
{
    vblank = {};
    lastStamp = 0.0;
}

void ModalOverlay::onVBlank (double nowSec)
{
    if (lastStamp <= 0.0)
        lastStamp = nowSec;

    const float dt = juce::jlimit (0.f, 0.05f, (float) (nowSec - lastStamp));
    lastStamp = nowSec;
    sequence.tick (dt);
    applyContentVisibility();
    repaint();
    finishIfClosed();
}

void ModalOverlay::finishIfClosed()
{
    if (! sequence.consumeFinished())
        return;

    stopVBlank();
    setVisible (false);
    if (onClose)
        onClose();
}

void ModalOverlay::applyContentVisibility()
{
    const float a = sequence.contentAlpha();
    const bool showKids = a > 0.02f
                       || sequence.getPhase() == OverlayPhase::Shown;

    if (content)
    {
        content->setVisible (showKids);
        content->setAlpha (a);
    }
    titleLabel.setVisible (showKids);
    titleLabel.setAlpha (a);
    okButton.setVisible (showKids && mode == OverlayMode::Decision);
    backButton.setVisible (showKids && mode == OverlayMode::Decision);
    closeButton.setVisible (showKids && (mode == OverlayMode::Closable || mode == OverlayMode::Decision));

    const auto phase = sequence.getPhase();
    if (showKids && (phase == OverlayPhase::EnterReveal || phase == OverlayPhase::EnterGlitch))
    {
        const int revealed = (int) std::round (sequence.timeline01() * (float) rawTitle.length());
        titleLabel.setText (decodeGlitchText (rawTitle, juce::jmax (0, revealed - 2), rng),
                            juce::dontSendNotification);
    }
    else if (showKids)
    {
        titleLabel.setText (rawTitle, juce::dontSendNotification);
    }
}

void ModalOverlay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const float t01 = sequence.timeline01();
    const float slice = sequence.sliceAmount();
    const float contentA = sequence.contentAlpha();
    const auto phase = sequence.getPhase();
    const bool exiting = phase == OverlayPhase::ExitGlitch || phase == OverlayPhase::ExitScrim;

    g.fillAll (juce::Colours::black.withAlpha (sequence.scrimAlpha()));
    CyberChrome::drawVignette (g, bounds, sequence.scrimAlpha());
    CyberChrome::drawScanlines (g, bounds, t01 * 8.f, 0.55f + 0.45f * sequence.scrimAlpha());
    CyberChrome::drawNoise (g, bounds, 0.08f + 0.22f * slice, rng.nextInt());
    CyberChrome::drawChromaticInset (g, bounds, 0.25f + 0.75f * slice);
    CyberChrome::drawGlitchSlices (g, bounds, slice,
                                    (int) (t01 * 997.f) ^ (int) (slice * 4096.f),
                                    kMaxGlitchSlices);

    const float wipe = exiting ? (1.f - sequence.wipeY01())
                               : (phase == OverlayPhase::EnterGlitch ? sequence.wipeY01()
                                                                     : juce::jmax (sequence.wipeY01(), contentA));
    CyberChrome::drawScanBeam (g, bounds, wipe, 0.45f + 0.55f * slice);

    const float build = exiting ? contentA : (phase == OverlayPhase::Shown ? 1.f
                                              : (phase == OverlayPhase::EnterReveal ? contentA
                                                                                    : wipe * 0.35f));
    if (build <= 0.01f)
        return;

    auto r = panel.toFloat();
    if (exiting)
        r = r.withHeight (r.getHeight() * juce::jmax (0.08f, contentA));
    else if (phase == OverlayPhase::EnterGlitch)
        r = r.withHeight (juce::jmax (8.f, r.getHeight() * wipe * 0.22f));
    else if (phase == OverlayPhase::EnterReveal)
        r = r.withHeight (r.getHeight() * juce::jmax (0.22f, contentA));

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (NeuroCoreLookAndFeel::background().withAlpha (0.55f + 0.45f * build));
        g.fillRect (r);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.9f * build));
        g.fillRect (r.getX(), r.getY(), r.getWidth(), 2.f);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.28f * build));
        g.fillRect (r.getX() + 10.f, r.getY() + 40.f, r.getWidth() - 20.f, 1.f);
        CyberChrome::drawScanlines (g, r.toNearestInt(), t01 * 12.f, 0.35f * build);
    }

    NeuroCoreLookAndFeel::drawHudFrame (g, r.reduced (0.5f), {});
    CyberChrome::drawHudCorners (g, r.expanded (3.f), build, 2.2f);

    auto meta = juce::Rectangle<int> (panel.getX() + 14,
                                      panel.getBottom() + 8,
                                      panel.getWidth() - 28,
                                      36);
    if (meta.getBottom() > getHeight() - 8)
        meta.setY (panel.getY() - 42);
    g.setFont (NeuroCoreLookAndFeel::monoFont (11.f));
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.7f * juce::jmax (build, slice)));
    g.drawText (CyberChrome::statusForProgress (exiting ? (1.f - t01) : t01),
                meta.removeFromTop (16), juce::Justification::centredLeft, false);
    CyberChrome::drawHexMeta (g, meta, exiting ? (1.f - t01) : t01);
}

void ModalOverlay::resized()
{
    const int pw = preferredW > 0 ? preferredW : juce::jmin (getWidth() - 48, (int) (getWidth() * 0.82f));
    const int ph = preferredH > 0 ? preferredH : juce::jmin (getHeight() - 48, (int) (getHeight() * 0.78f));
    panel = getLocalBounds().withSizeKeepingCentre (pw, ph);

    auto area = panel.reduced (14, 12);
    auto header = area.removeFromTop (32);
    closeButton.setBounds (header.removeFromRight (36).reduced (2));
    if (mode == OverlayMode::Decision)
    {
        okButton.setBounds (header.removeFromRight (64).reduced (2));
        backButton.setBounds (header.removeFromRight (64).reduced (2));
    }
    titleLabel.setBounds (header);

    area.removeFromTop (8);

    if (mode == OverlayMode::Decision)
    {
        auto buttonArea = area.removeFromBottom (36);
        okButton.setBounds (buttonArea.removeFromRight (90).reduced (3));
        backButton.setBounds (buttonArea.removeFromRight (90).reduced (3));
    }

    if (content)
        content->setBounds (area);
}

void ModalOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (mode == OverlayMode::Closable && ! panel.contains (e.getPosition()))
        requestClose();
}

bool ModalOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (mode != OverlayMode::Blocking)
            requestClose();
        return true;
    }
    if (key == juce::KeyPress::returnKey && mode == OverlayMode::Decision)
    {
        if (onAccept)
            onAccept();
        return true;
    }
    return false;
}

void ModalOverlay::updateButtonVisibility()
{
    applyContentVisibility();
}
