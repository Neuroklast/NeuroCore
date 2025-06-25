#include "CircularProgressBar.h"

using namespace juce;

CircularProgressBar::CircularProgressBar(std::atomic<float>& v)
    : value(v)
{
    startTimerHz(30);
}

void CircularProgressBar::paint(Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto radius = jmin(area.getWidth(), area.getHeight()) / 2.0f;
    auto centre = area.getCentre();

    g.setColour(Colours::darkgrey);
    g.fillEllipse(area);
    g.setColour(Colours::white);
    g.drawEllipse(area, 2.0f);

    float progress = jlimit(0.0f, 1.0f, value.load());
    if (progress > 0.0f)
    {
        Path p;
        p.addPieSegment(area.reduced(4.0f), -MathConstants<float>::halfPi,
                        -MathConstants<float>::halfPi + progress * MathConstants<float>::twoPi,
                        0.8f);
        g.setColour(Colours::orange);
        g.fillPath(p);
    }
}

void CircularProgressBar::timerCallback()
{
    repaint();
}
