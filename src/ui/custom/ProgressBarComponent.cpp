#include "ProgressBarComponent.h"

using namespace juce;

namespace ui
{
    ProgressBarComponent::ProgressBarComponent()
    {
        startTimerHz(30);
    }

    ProgressBarComponent::~ProgressBarComponent()
    {
        stopTimer();
    }

    void ProgressBarComponent::setProgress(float newValue) noexcept
    {
        progress.store(jlimit(0.0f, 1.0f, newValue));
    }

    float ProgressBarComponent::getProgress() const noexcept
    {
        return progress.load();
    }

    void ProgressBarComponent::setColours(Colour bar, Colour background)
    {
        barColour = bar;
        backgroundColour = background;
        repaint();
    }

    void ProgressBarComponent::paint(Graphics& g)
    {
        auto area = getLocalBounds().toFloat();
        g.setColour(backgroundColour);
        g.fillRoundedRectangle(area, 3.0f);

        g.setColour(barColour);
        Rectangle<float> bar(area.withWidth(area.getWidth() * progressDisplay));
        g.fillRoundedRectangle(bar, 3.0f);

        g.setColour(barColour.contrasting());
        g.drawText(juce::String(juce::roundToInt(progressDisplay * 100.0f)) + "%",
                   getLocalBounds(), juce::Justification::centred);
    }

    void ProgressBarComponent::resized()
    {
    }

    void ProgressBarComponent::timerCallback()
    {
        auto value = progress.load();
        if (value != progressDisplay)
        {
            progressDisplay = value;
            repaint();
        }
    }
} // namespace ui

