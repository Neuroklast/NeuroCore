#include "StereoFieldComponent.h"
#include "PluginLookAndFeel.h"

StereoFieldComponent::StereoFieldComponent (NeuroCoreAudioProcessor& proc,
                                            WaveformDisplayComponent::Type t)
    : processor (proc), type (t)
{
    setOpaque (true);
    setTooltip ("Stereo field. Up = left+right together. Sides = left vs right.");
    startTimerHz (30);
}

StereoFieldComponent::~StereoFieldComponent()
{
    stopTimer();
}

void StereoFieldComponent::timerCallback()
{
    if (type == WaveformDisplayComponent::Type::Input)
        processor.getInputWaveform (buffer);
    else
        processor.getOutputWaveform (buffer);

    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0 || nCh <= 0)
        return;

    const float* L = buffer.getReadPointer (0);
    const float* R = nCh > 1 ? buffer.getReadPointer (1) : L;
    stats = ScopeAnalytics::analyse (L, R, n);
    repaint();
}

void StereoFieldComponent::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::black);

    auto plot = full.reduced (6.f, 16.f);
    plot.removeFromBottom (14.f);
    const float side = juce::jmin (plot.getWidth(), plot.getHeight());
    plot = juce::Rectangle<float> (0.f, 0.f, side, side).withCentre (plot.getCentre());

    juce::ColourGradient well (juce::Colour (0xff120202), plot.getCentreX(), plot.getY(),
                               juce::Colour (0xff000000), plot.getCentreX(), plot.getBottom(), false);
    g.setGradientFill (well);
    g.fillRect (plot);

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f));
    g.drawRect (plot, 1.f);

    const auto c = plot.getCentre();
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.22f));
    g.drawLine (c.x, plot.getY(), c.x, plot.getBottom(), 0.6f);
    g.drawLine (plot.getX(), c.y, plot.getRight(), c.y, 0.6f);
    g.drawLine (plot.getX(), plot.getY(), plot.getRight(), plot.getBottom(), 0.5f);
    g.drawLine (plot.getRight(), plot.getY(), plot.getX(), plot.getBottom(), 0.5f);

    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n >= 2 && nCh > 0)
    {
        const float* L = buffer.getReadPointer (0);
        const float* R = nCh > 1 ? buffer.getReadPointer (1) : L;
        const int step = juce::jmax (1, n / 256);
        juce::Path path;
        bool started = false;
        const float hx = plot.getWidth() * 0.5f;
        const float hy = plot.getHeight() * 0.5f;
        for (int i = 0; i < n; i += step)
        {
            const auto p = ScopeAnalytics::gonioPoint (L[i], R[i]);
            const float x = juce::jlimit (plot.getX(), plot.getRight(),
                                          c.x + p.x * hx);
            const float y = juce::jlimit (plot.getY(), plot.getBottom(),
                                          c.y + p.y * hy);
            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.18f));
        g.strokePath (path, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.92f));
        g.strokePath (path, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.80f));
    g.setFont (NeuroCoreLookAndFeel::monoFont (9.f));
    g.drawText (type == WaveformDisplayComponent::Type::Input ? "IN FIELD" : "OUT FIELD",
                full.getX() + 4.f, full.getY() + 2.f, full.getWidth() - 8.f, 12.f,
                juce::Justification::centredLeft, false);

    auto corrBar = juce::Rectangle<float> (plot.getX(), full.getBottom() - 12.f,
                                           plot.getWidth(), 6.f);
    g.setColour (juce::Colour (0xff1a0505));
    g.fillRect (corrBar);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.45f));
    g.drawRect (corrBar, 1.f);
    const float midX = corrBar.getCentreX();
    const float corr = juce::jlimit (-1.f, 1.f, stats.correlation);
    const float markX = midX + corr * (corrBar.getWidth() * 0.5f - 2.f);
    g.setColour (NeuroCoreLookAndFeel::accent());
    g.fillRect (juce::jmin (midX, markX), corrBar.getY() + 1.f,
                std::abs (markX - midX), corrBar.getHeight() - 2.f);
    g.fillRect (markX - 1.f, corrBar.getY(), 2.f, corrBar.getHeight());
}
