#include "ModalOverlay.h"
#include "PluginLookAndFeel.h"

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

void ModalOverlay::setMode (OverlayMode newMode)
{
    mode = newMode;
    updateButtonVisibility();
}

void ModalOverlay::setContent (std::unique_ptr<juce::Component> newContent)
{
    if (content)
        removeChildComponent (content.get());
    content = std::move (newContent);
    if (content)
        addAndMakeVisible (*content);
    resized();
}

void ModalOverlay::setTitle (const juce::String& text)
{
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
    // Child of editor — no desktop peer (avoids host UI lock / focus theft)
    parent.addAndMakeVisible (this);
    setBounds (parent.getLocalBounds());
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    resized();
}

void ModalOverlay::requestClose()
{
    if (onClose)
        onClose();
}

void ModalOverlay::paint (juce::Graphics& g)
{
    // Heavy black scrim (terminal modal)
    g.fillAll (juce::Colours::black.withAlpha (0.72f));

    const auto r = panel.toFloat();
    g.setColour (NeuroCoreLookAndFeel::background());
    g.fillRect (r);
    NeuroCoreLookAndFeel::drawHudFrame (g, r.reduced (0.5f), {});
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.9f));
    g.fillRect (panel.getX(), panel.getY(), panel.getWidth(), 2);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.3f));
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
    // Click outside card closes Closable overlays
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
    closeButton.setVisible (mode == OverlayMode::Closable || mode == OverlayMode::Decision);
    okButton.setVisible (mode == OverlayMode::Decision);
    backButton.setVisible (mode == OverlayMode::Decision);
}
