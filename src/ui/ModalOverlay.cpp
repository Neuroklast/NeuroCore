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

void ModalOverlay::setLiveStatus (const juce::String& text)
{
    liveStatus = text;
    repaint();
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
    clipType = randomClipReveal (rng);
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
    applyClipLayout();
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
    const bool showChrome = sequence.clipProgress() > 0.22f;

    if (content)
    {
        content->setVisible (showKids);
        content->setAlpha (a);
    }
    titleLabel.setVisible (showChrome);
    titleLabel.setAlpha (1.f);
    okButton.setVisible (showKids && mode == OverlayMode::Decision);
    backButton.setVisible (showKids && mode == OverlayMode::Decision);
    closeButton.setVisible (showChrome && (mode == OverlayMode::Closable || mode == OverlayMode::Decision));

    if (showChrome && sequence.isLoaderVisible())
    {
        const int revealed = (int) std::round (sequence.clipProgress() * (float) rawTitle.length());
        titleLabel.setText (decodeGlitchText (rawTitle, revealed, rng),
                            juce::dontSendNotification);
    }
    else if (showChrome)
    {
        titleLabel.setText (rawTitle, juce::dontSendNotification);
    }
}

void ModalOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (sequence.scrimAlpha()));

    const float burst = sequence.sliceAmount();
    if (burst > 0.04f)
        CyberChrome::drawGlitchSlices (g, getLocalBounds(), burst,
                                        (int) (sequence.timeSec() * 997.f),
                                        kMaxGlitchSlices);

    if (sequence.clipProgress() <= 0.02f)
        return;

    const auto r = panel.toFloat();
    g.setColour (NeuroCoreLookAndFeel::background());
    g.fillRect (r);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.45f));
    g.drawRect (r, 1.f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.14f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 40.f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.90f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 2.f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.30f));
    g.fillRect (r.getX() + 10.f, r.getY() + 40.f, r.getWidth() - 20.f, 1.f);
    CyberChrome::drawHudCorners (g, r.expanded (2.f), sequence.clipProgress(), 2.4f);

    const bool blink = ((int) (sequence.timeSec() * 6.f) % 2) == 0;
    g.setColour (blink ? NeuroCoreLookAndFeel::accent()
                       : NeuroCoreLookAndFeel::accent().withAlpha (0.25f));
    g.fillEllipse (r.getX() + 10.f, r.getY() + 14.f, 6.f, 6.f);

    if (sequence.isLoaderVisible())
    {
        auto inner = panel.reduced (8, 46);
        inner.removeFromBottom (8);
        const bool teardown = sequence.getPhase() == OverlayPhase::ExitGlitch
                           || sequence.getPhase() == OverlayPhase::ExitScrim;
        CyberChrome::drawOverlayLoader (g, inner, sequence.timeSec(),
                                        sequence.timeline01(),
                                        (int) clipType, rng.nextInt(), teardown);
        if (liveStatus.isNotEmpty())
        {
            g.setFont (NeuroCoreLookAndFeel::monoFont (11.f));
            g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.85f));
            g.drawText (liveStatus, inner.removeFromBottom (18).reduced (12, 0),
                        juce::Justification::centredLeft, false);
        }
    }

    if (burst > 0.04f)
        CyberChrome::drawGlitchSlices (g, panel, burst * 0.7f,
                                        (int) (sequence.clipProgress() * 4093.f), 4);
}

void ModalOverlay::applyClipLayout()
{
    const int pw = preferredW > 0 ? preferredW : juce::jmin (getWidth() - 48, (int) (getWidth() * 0.82f));
    const int ph = preferredH > 0 ? preferredH : juce::jmin (getHeight() - 48, (int) (getHeight() * 0.78f));
    targetPanel = getLocalBounds().withSizeKeepingCentre (pw, ph);
    panel = revealBounds (targetPanel, clipType, sequence.clipProgress());

    auto area = panel.reduced (14, 12);
    auto header = area.removeFromTop (32);
    header.removeFromLeft (16);
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

void ModalOverlay::resized()
{
    applyClipLayout();
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
