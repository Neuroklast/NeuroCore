#include "LoudnessMeterComponent.h"

LoudnessMeterComponent::LoudnessMeterComponent(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(30);
}

void LoudnessMeterComponent::timerCallback()
{
    loudness = processor.getLoudnessDb();
    limiter  = processor.isLimiterActive();
    if (processor.consumeInvalidFlag())
        blinkCount = 6; // roughly 200 ms at 30 Hz

    if (blinkCount > 0)
    {
        blink = !blink;
        --blinkCount;
    }
    else
    {
        blink = false;
    }
    repaint();
}

void LoudnessMeterComponent::drawLed(juce::Graphics& g,
                                     juce::Rectangle<float> area, bool on)
{
    g.setColour(on ? juce::Colours::yellow : juce::Colours::darkgrey);
    g.fillEllipse(area);
}

void LoudnessMeterComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds().toFloat();
    auto ledArea = juce::Rectangle<float>(bounds.getWidth() - 15.0f, 5.0f, 10.0f, 10.0f);
    drawLed(g, ledArea, blink);

    auto meterArea = bounds.reduced(10.0f, 20.0f);
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(meterArea);
    g.setColour(juce::Colours::white);
    g.drawRect(meterArea);

    float level = juce::jlimit(0.0f, 1.0f, (loudness + 60.0f) / 60.0f);
    auto fill = meterArea.withY(meterArea.getBottom() - meterArea.getHeight() * level)
                         .withHeight(meterArea.getHeight() * level);
    g.setColour(limiter ? juce::Colours::red : juce::Colours::green);
    g.fillRect(fill);

    g.setColour(juce::Colours::white);
    g.drawFittedText(juce::String(loudness, 1) + " dB", meterArea.toNearestInt(),
                     juce::Justification::centredBottom, 1);
}

