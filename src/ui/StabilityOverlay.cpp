#include "StabilityOverlay.h"

StabilityOverlay::StabilityOverlay(const juce::String& message)
    : text(message)
{
    setInterceptsMouseClicks(true, true);
    okButton.onClick = [this] { exit(); };
    addAndMakeVisible(okButton);
}

void StabilityOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.6f));

    g.setColour(juce::Colours::darkgrey);
    g.fillRect(messageArea);
    g.setColour(juce::Colours::white);
    g.drawRect(messageArea);

    auto textArea = messageArea.reduced(10);
    textArea.removeFromBottom(40);
    g.drawFittedText(text, textArea, juce::Justification::centred, 3);
}

void StabilityOverlay::resized()
{
    auto area = getLocalBounds();
    int w = area.getWidth() * 6 / 10;
    int h = area.getHeight() * 3 / 10;
    messageArea = juce::Rectangle<int>((area.getWidth() - w) / 2,
                                       (area.getHeight() - h) / 2,
                                       w, h);

    int btnW = 80;
    int btnH = 30;
    okButton.setBounds(messageArea.getRight() - btnW - 10,
                       messageArea.getBottom() - btnH - 10,
                       btnW, btnH);
}

void StabilityOverlay::exit()
{
    if (auto* parent = getParentComponent())
        parent->removeChildComponent(this);
    if (onDismiss)
        onDismiss();
}
