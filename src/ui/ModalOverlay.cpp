#include "ModalOverlay.h"
#include "PluginLookAndFeel.h"
#include "fx/DecodeText.h"

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

    if (showKids && sequence.getPhase() == OverlayPhase::EnterReveal)
    {
        const int revealed = (int) std::round (a * (float) rawTitle.length());
        titleLabel.setText (decodeGlitchText (rawTitle, revealed, rng),
                            juce::dontSendNotification);
    }
    else if (showKids)
    {
        titleLabel.setText (rawTitle, juce::dontSendNotification);
    }
}

void ModalOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (sequence.scrimAlpha()));

    const float slice = sequence.sliceAmount();
    if (slice > 0.05f)
    {
        const int y = (int) (sequence.wipeY01() * (float) juce::jmax (1, getHeight() - 24));
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.16f * slice));
        g.fillRect (0, y, getWidth(), 12);
        g.setColour (juce::Colour (0x44ff0044));
        g.fillRect (3, y + 2, getWidth(), 2);
        g.setColour (juce::Colour (0x4400ffff));
        g.fillRect (-3, y + 6, getWidth(), 2);
    }

    if (sequence.contentAlpha() <= 0.02f
        && sequence.getPhase() != OverlayPhase::Shown)
        return;

    const auto r = panel.toFloat();
    g.setColour (NeuroCoreLookAndFeel::background().withAlpha (sequence.contentAlpha()));
    g.fillRect (r);
    NeuroCoreLookAndFeel::drawHudFrame (g, r.reduced (0.5f), {});
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.9f * sequence.contentAlpha()));
    g.fillRect (panel.getX(), panel.getY(), panel.getWidth(), 2);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.3f * sequence.contentAlpha()));
    g.fillRect (panel.getX() + 10, panel.getY() + 40, panel.getWidth() - 20, 1);
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
