#include "ModalOverlay.h"

ModalOverlay::ModalOverlay()
{
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(acceptButton);
    addAndMakeVisible(backButton);

    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

    acceptButton.onClick = [this]
    {
        if (onAccept)
            onAccept();
    };

    backButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

    setMode(OverlayMode::Blocking);
}

void ModalOverlay::setTitle(const juce::String& text)
{
    title = text;
    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
}

void ModalOverlay::setMode(OverlayMode m)
{
    mode = m;
    bool isClosable = (mode == OverlayMode::Closable);
    bool isDecision = (mode == OverlayMode::Decision);

    closeButton.setVisible(isClosable);
    acceptButton.setVisible(isDecision);
    backButton.setVisible(isDecision);
    resized();
}

void ModalOverlay::setContent(std::unique_ptr<juce::Component> c)
{
    if (content)
        removeChildComponent(content.get());
    content = std::move(c);
    if (content)
        addAndMakeVisible(*content);
    resized();
}

void ModalOverlay::show(juce::Component& parent)
{
    parent.addAndMakeVisible(*this);
    setBounds(parent.getLocalBounds());
    toFront(true);
    grabKeyboardFocus();
}

void ModalOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.5f));
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(panel);
    g.setColour(juce::Colours::white);
    g.drawRect(panel, 1);
}

void ModalOverlay::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth() * 6 / 10,
                                                  getHeight() * 6 / 10);
    auto area = panel.reduced(8);

    auto titleHeight = 24;
    if (!title.isEmpty())
        titleLabel.setBounds(area.removeFromTop(titleHeight));
    else
        titleLabel.setBounds(0, 0, 0, 0);

    auto buttonHeight = 24;
    if (mode == OverlayMode::Decision)
    {
        auto buttons = area.removeFromBottom(buttonHeight);
        acceptButton.setBounds(buttons.removeFromRight(80).reduced(2));
        backButton.setBounds(buttons.removeFromRight(80).reduced(2));
    }
    else if (mode == OverlayMode::Closable)
    {
        auto buttons = area.removeFromBottom(buttonHeight);
        closeButton.setBounds(buttons.removeFromRight(80).reduced(2));
    }
    else
    {
        closeButton.setBounds(0, 0, 0, 0);
        acceptButton.setBounds(0, 0, 0, 0);
        backButton.setBounds(0, 0, 0, 0);
    }

    if (content)
        content->setBounds(area);
}

bool ModalOverlay::keyPressed(const juce::KeyPress& kp)
{
    if (mode == OverlayMode::Closable || mode == OverlayMode::Decision)
    {
        if (kp == juce::KeyPress::escapeKey)
        {
            if (onClose)
                onClose();
            return true;
        }
        if (mode == OverlayMode::Decision && kp == juce::KeyPress::returnKey)
        {
            if (onAccept)
                onAccept();
            return true;
        }
    }
    return true; // consume all keys while overlay is shown
}
