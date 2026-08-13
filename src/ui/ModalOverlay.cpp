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
    g.fillAll (juce::Colours::black.withAlpha (sequence.scrimAlpha()));

    const float burst = sequence.sliceAmount();
    if (burst > 0.04f)
        CyberChrome::drawGlitchSlices (g, getLocalBounds(), burst,
                                        (int) (sequence.timeline01() * 997.f),
                                        kMaxGlitchSlices);

    if (sequence.clipProgress() <= 0.02f)
        return;

    const auto r = panel.toFloat();
    g.setColour (NeuroCoreLookAndFeel::background());
    g.fillRect (r);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.40f));
    g.drawRect (r, 1.f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.18f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 40.f);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.85f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 2.f);
    CyberChrome::drawHudCorners (g, r.expanded (2.f), sequence.clipProgress(), 2.4f);

    if (burst > 0.04f)
        CyberChrome::drawGlitchSlices (g, panel, burst * 0.8f,
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
