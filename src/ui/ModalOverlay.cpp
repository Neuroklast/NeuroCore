#include "ModalOverlay.h"

ModalOverlay::ModalOverlay()
{
    setInterceptsMouseClicks(true, true);
    setOpaque(false);
    setAlwaysOnTop(true);
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(okButton);
    addAndMakeVisible(backButton);
    addAndMakeVisible(closeButton);

    okButton.onClick = [this]{ if(onAccept) onAccept(); };
    backButton.onClick = [this]{ if(onClose) onClose(); };
    closeButton.onClick = [this]{ if(onClose) onClose(); };

    updateButtonVisibility();
}

void ModalOverlay::setMode(OverlayMode newMode)
{
    mode = newMode;
    updateButtonVisibility();
}

void ModalOverlay::setContent(std::unique_ptr<juce::Component> newContent)
{
    if (content)
        removeChildComponent(content.get());
    content = std::move(newContent);
    if (content)
        addAndMakeVisible(*content);
    resized();
}

void ModalOverlay::setTitle(const juce::String& text)
{
    titleLabel.setText(text, juce::dontSendNotification);
    resized();
}

void ModalOverlay::show(juce::Component& parent)
{
    setOpaque(false);
    setInterceptsMouseClicks(true, true);

    if (getPeer() == nullptr)
    {
        int flags = juce::ComponentPeer::windowIsTemporary
            | juce::ComponentPeer::windowIsSemiTransparent;

        addToDesktop(flags);
        setAlwaysOnTop(true);
    }

    setBounds(parent.getScreenBounds());

    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
}



void ModalOverlay::paint(juce::Graphics& g)
{
    if (auto* lf = dynamic_cast<juce::LookAndFeel_V4*>(&getLookAndFeel()))
    {
        const auto& scheme = lf->getCurrentColourScheme();
        g.fillAll(scheme.getUIColour(juce::LookAndFeel_V4::ColourScheme::windowBackground).withAlpha(0.5f));
        g.setColour(scheme.getUIColour(juce::LookAndFeel_V4::ColourScheme::widgetBackground));
        g.fillRect(panel);
        g.setColour(scheme.getUIColour(juce::LookAndFeel_V4::ColourScheme::outline));
        g.drawRect(panel);
    }
    else
    {
        g.fillAll(juce::Colours::black.withAlpha(0.5f));
        g.setColour(juce::Colours::darkgrey);
        g.fillRect(panel);
        g.setColour(juce::Colours::white);
        g.drawRect(panel);
    }
}

void ModalOverlay::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth()*6/10, getHeight()*6/10);
    auto area = panel.reduced(8);
    titleLabel.setBounds(area.removeFromTop(24));

    auto buttonArea = area.removeFromBottom(28);
    okButton.setBounds(buttonArea.removeFromRight(60).reduced(2));
    backButton.setBounds(buttonArea.removeFromRight(60).reduced(2));
    closeButton.setBounds(buttonArea.removeFromLeft(60).reduced(2));

    if (content)
        content->setBounds(area);
}

void ModalOverlay::updateButtonVisibility()
{
    closeButton.setVisible(mode == OverlayMode::Closable);
    okButton.setVisible(mode == OverlayMode::Decision);
    backButton.setVisible(mode == OverlayMode::Decision);
}
