#include "ProgressOverlay.h"

using namespace juce;

ProgressOverlay::ProgressOverlay(const String& message, std::atomic<float>& progress)
    : text(message), progressValue(progress), progressBar(progress)
{
    setInterceptsMouseClicks(true, true);
    addAndMakeVisible(progressBar);
}

void ProgressOverlay::paint(Graphics& g)
{
    g.fillAll(Colours::black.withAlpha(0.6f));

    g.setColour(Colours::darkgrey);
    g.fillRect(messageArea);
    g.setColour(Colours::white);
    g.drawRect(messageArea);

    g.drawFittedText(text, messageArea.reduced(10).removeFromTop(30),
                     Justification::centred, 1);
}

void ProgressOverlay::resized()
{
    auto area = getLocalBounds();
    int w = area.getWidth() * 4 / 10;
    int h = area.getHeight() * 3 / 10;
    messageArea = Rectangle<int>((area.getWidth() - w) / 2,
                                 (area.getHeight() - h) / 2,
                                 w, h);
    auto barArea = messageArea.reduced(20);
    barArea.removeFromTop(30);
    progressBar.setBounds(barArea);
}
