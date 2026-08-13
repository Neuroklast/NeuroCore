#include <JuceHeader.h>
#include "WaveformDisplayComponent.h"
#include <algorithm>
#include <cmath>
#include "../dsp/DSPUtils.h"
#include "../utils/Localiser.h"
#include "PluginLookAndFeel.h"

WaveformDisplayComponent::WaveformDisplayComponent (NeuroCoreAudioProcessor& proc, Type t)
    : processor (proc), type (t)
{
    setOpaque (true);
    displayData.assign ((size_t) Config::kWaveformDisplaySamples, 0.f);
    monoScratch.assign ((size_t) Config::kWaveformDisplaySamples, 0.f);
    startTimerHz (30);
}

WaveformDisplayComponent::~WaveformDisplayComponent()
{
    stopTimer();
}

juce::Rectangle<float> WaveformDisplayComponent::plotArea() const
{
    return getLocalBounds().toFloat()
        .withTrimmedLeft (48.0f)
        .withTrimmedRight (12.0f)
        .withTrimmedTop (18.0f)
        .withTrimmedBottom (36.0f);
}

void WaveformDisplayComponent::fillMonoFromBuffer (std::vector<float>& dest) const
{
    const int num = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (num <= 0 || nCh <= 0)
        return;

    if ((int) dest.size() != num)
        dest.assign ((size_t) num, 0.f);

    const float* L = buffer.getReadPointer (0);
    const float* R = nCh > 1 ? buffer.getReadPointer (1) : nullptr;

    if (R == nullptr)
    {
        for (int i = 0; i < num; ++i)
            dest[(size_t) i] = L[i];
        return;
    }

    // Energy-weighted mono: if one side is silent (input router L/R only), still show signal.
    // Pure average would look "half height" on mono sources in one channel.
    for (int i = 0; i < num; ++i)
    {
        const float l = L[i];
        const float r = R[i];
        const float al = std::abs (l);
        const float ar = std::abs (r);
        if (al < 1.0e-6f && ar < 1.0e-6f)
            dest[(size_t) i] = 0.f;
        else if (al >= ar * 4.0f)
            dest[(size_t) i] = l; // L dominates
        else if (ar >= al * 4.0f)
            dest[(size_t) i] = r; // R dominates
        else
            dest[(size_t) i] = 0.5f * (l + r);
    }
}

void WaveformDisplayComponent::timerCallback()
{
    if (type == Type::Input)
        processor.getInputWaveform (buffer);
    else
        processor.getOutputWaveform (buffer);

    const int num = buffer.getNumSamples();
    if (num <= 0)
        return;

    fillMonoFromBuffer (monoScratch);

    if ((int) displayData.size() != num)
        displayData.assign ((size_t) num, 0.f);

    // Time alignment: IN discovers rising zero-cross; OUT reuses the same offset so
    // both scopes show the same window of the ring buffer (true before/after).
    int align = 0;
    if (fixedWave)
    {
        if (type == Type::Input)
        {
            for (int i = 1; i < num; ++i)
            {
                if (monoScratch[(size_t) (i - 1)] < 0.0f && monoScratch[(size_t) i] >= 0.0f)
                {
                    align = i;
                    break;
                }
            }
            processor.setWaveformAlignOffset (align);
        }
        else
        {
            align = processor.getWaveformAlignOffset() % juce::jmax (1, num);
        }
    }

    // Light IIR toward new ring data (stable, no SmoothedValue drain)
    for (int i = 0; i < num; ++i)
    {
        const float s = monoScratch[(size_t) ((i + align) % num)];
        displayData[(size_t) i] = displayData[(size_t) i] * 0.65f + s * 0.35f;
    }

    if (xScale == XScale::Frequency)
    {
        // FFT on aligned mono window
        juce::AudioBuffer<float> monoBuf (1, num);
        for (int i = 0; i < num; ++i)
            monoBuf.setSample (0, i, monoScratch[(size_t) ((i + align) % num)]);
        juce::dsp::AudioBlock<float> block (monoBuf);
        DSPUtils::analyseFFT (block, 0, fftMagnitudes, fftOrder);
    }

    repaint();
}

float WaveformDisplayComponent::indexToX (int i, int total, juce::Rectangle<float> area) const
{
    const auto norm = static_cast<float> (i) / static_cast<float> (juce::jmax (1, total - 1));
    return area.getX() + norm * area.getWidth();
}

float WaveformDisplayComponent::valueToY (float v, juce::Rectangle<float> area) const
{
    float scaled = juce::jlimit (-1.0f, 1.0f, v);
    if (yScale == YScale::Decibel)
    {
        const float db = 20.0f * std::log10 (juce::jmax (1.0e-6f, std::abs (scaled)));
        scaled = juce::jlimit (-60.0f, 0.0f, db) / 60.0f;
        if (v < 0.0f)
            scaled = -scaled;
    }
    float norm = 0.5f - 0.5f * scaled;
    if (invertY)
        norm = 1.0f - norm;
    return area.getY() + norm * area.getHeight();
}

void WaveformDisplayComponent::drawAxes (juce::Graphics& g, juce::Rectangle<float> plot)
{
    // Depth well
    juce::ColourGradient well (juce::Colour (0xff120202), plot.getCentreX(), plot.getY(),
                               juce::Colour (0xff000000), plot.getCentreX(), plot.getBottom(), false);
    g.setGradientFill (well);
    g.fillRect (plot);

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f));
    g.drawRect (plot, 1.0f);

    const int totalSamples = (int) displayData.size();
    const int visibleSamples = juce::jlimit (1, totalSamples, static_cast<int> (totalSamples / zoom));
    const int xTicks = 8;
    const int yTicks = 4;

    g.setFont (NeuroCoreLookAndFeel::monoFont (9.f));

    for (int i = 0; i <= xTicks; ++i)
    {
        const float x = plot.getX() + (plot.getWidth() / (float) xTicks) * (float) i;
        if (showGrid)
        {
            g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (i == 0 || i == xTicks ? 0.35f : 0.12f));
            g.drawLine (x, plot.getY(), x, plot.getBottom(), 0.6f);
        }
        g.setColour (juce::Colour (0xff888888));
        double value = 0.0;
        switch (xScale)
        {
            case XScale::Samples:
                value = juce::jmap ((double) i, 0.0, (double) xTicks, 0.0, (double) (visibleSamples - 1));
                g.drawFittedText (juce::String ((int) value), (int) x - 24, (int) plot.getBottom() + 2, 48, 12,
                                  juce::Justification::centred, 1);
                break;
            case XScale::Time:
                value = juce::jmap ((double) i, 0.0, (double) xTicks, 0.0,
                                    (visibleSamples - 1) / juce::jmax (1.0, processor.getSampleRate())) * 1000.0;
                g.drawFittedText (juce::String (value, 0) + "ms", (int) x - 24, (int) plot.getBottom() + 2, 48, 12,
                                  juce::Justification::centred, 1);
                break;
            case XScale::Frequency:
                value = juce::jmap ((double) i, 0.0, (double) xTicks, 0.0, processor.getSampleRate() / 2.0);
                g.drawFittedText (juce::String (value, 0), (int) x - 24, (int) plot.getBottom() + 2, 48, 12,
                                  juce::Justification::centred, 1);
                break;
        }
    }

    for (int i = 0; i <= yTicks; ++i)
    {
        const float y = plot.getY() + (plot.getHeight() / (float) yTicks) * (float) i;
        if (showGrid)
        {
            g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.12f));
            g.drawLine (plot.getX(), y, plot.getRight(), y, 0.6f);
        }
        g.setColour (juce::Colour (0xff888888));
        const double value = juce::jmap ((double) i, 0.0, (double) yTicks, 1.0, -1.0);
        g.drawFittedText (juce::String (value, 1), (int) plot.getX() - 44, (int) y - 7, 40, 14,
                          juce::Justification::centredRight, 1);
    }

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.75f));
    g.setFont (NeuroCoreLookAndFeel::monoFont (10.f));
    // PRE = host buffer before DSP, POST = after full chain (true before/after)
    g.drawText (type == Type::Input ? "IN // PRE" : "OUT // POST",
                plot.getX() + 4.f, plot.getY() + 2.f, 96.f, 12.f,
                juce::Justification::centredLeft, false);
}

void WaveformDisplayComponent::drawScope (juce::Graphics& g, juce::Rectangle<float> plot)
{
    const int total = (xScale == XScale::Frequency && ! fftMagnitudes.empty())
                        ? (int) fftMagnitudes.size()
                        : (int) displayData.size();
    if (total < 2 || plot.getWidth() < 4.f || plot.getHeight() < 4.f)
        return;

    const int num = juce::jlimit (2, total, (int) (total / zoom));

    juce::Path path;
    bool started = false;
    for (int i = 0; i < num; ++i)
    {
        float value = 0.f;
        if (xScale == XScale::Frequency && ! fftMagnitudes.empty())
            value = fftMagnitudes[(size_t) juce::jmin (i, (int) fftMagnitudes.size() - 1)];
        else
            value = displayData[(size_t) i];

        if (! std::isfinite (value))
            value = 0.f;

        const float x = indexToX (i, num, plot);
        const float y = valueToY (value, plot);
        if (! started)
        {
            path.startNewSubPath (x, y);
            started = true;
        }
        else
            path.lineTo (x, y);
    }

    // Soft glow underlay (depth / bloom)
    g.setColour (lineColour.withAlpha (0.12f));
    g.strokePath (path, juce::PathStrokeType (lineThickness + 6.f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (lineColour.withAlpha (0.28f));
    g.strokePath (path, juce::PathStrokeType (lineThickness + 3.f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    // Core trace
    g.setColour (lineColour.withAlpha (0.95f));
    g.strokePath (path, juce::PathStrokeType (lineThickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    // Hot white core
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.strokePath (path, juce::PathStrokeType (juce::jmax (0.6f, lineThickness * 0.35f),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void WaveformDisplayComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // Subtle radial depth behind plot
    auto full = getLocalBounds().toFloat();
    juce::ColourGradient vignette (juce::Colour (0xff1a0505), full.getCentreX(), full.getCentreY(),
                                   juce::Colours::black, full.getCentreX(), full.getBottom(), true);
    g.setGradientFill (vignette);
    g.fillRect (full);

    const auto plot = plotArea();
    if (plot.getWidth() < 8.f || plot.getHeight() < 8.f)
        return;

    drawAxes (g, plot);
    drawScope (g, plot);

    // Soft CRT scanlines (low alpha — depth, not noise)
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    for (float y = plot.getY(); y < plot.getBottom(); y += 3.f)
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
}

void WaveformDisplayComponent::updateTooltip (juce::Point<int> pos, juce::Rectangle<float> area)
{
    const int total = (xScale == XScale::Frequency && ! fftMagnitudes.empty())
                        ? (int) fftMagnitudes.size()
                        : (int) displayData.size();
    const int num = juce::jlimit (1, juce::jmax (1, total), (int) (total / zoom));
    const int index = juce::jlimit (0, num - 1,
        (int) ((pos.x - area.getX()) / juce::jmax (1.f, area.getWidth()) * (float) num));

    float value = 0.f;
    if (xScale == XScale::Frequency && ! fftMagnitudes.empty())
        value = fftMagnitudes[(size_t) juce::jmin (index, (int) fftMagnitudes.size() - 1)];
    else if (! displayData.empty())
        value = displayData[(size_t) juce::jmin (index, (int) displayData.size() - 1)];

    juce::String xStr = juce::String (index);
    if (xScale == XScale::Time && processor.getSampleRate() > 0)
        xStr = juce::String ((index / processor.getSampleRate()) * 1000.0, 2) + " ms";
    else if (xScale == XScale::Frequency)
        xStr = juce::String ((index / (float) juce::jmax (1, (int) fftMagnitudes.size()))
                             * (float) (processor.getSampleRate() * 0.5), 1) + " Hz";

    setTooltip (xStr + ", " + juce::String (value, 3));
}

void WaveformDisplayComponent::mouseMove (const juce::MouseEvent& e)
{
    updateTooltip (e.position.toInt(), plotArea());
}

void WaveformDisplayComponent::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown())
        return;

    juce::PopupMenu menu;
    juce::PopupMenu xMenu, yMenu;
    xMenu.addItem (1, TRANS ("ScaleSamples"), true, xScale == XScale::Samples);
    xMenu.addItem (2, TRANS ("ScaleTime"), true, xScale == XScale::Time);
    xMenu.addItem (3, TRANS ("ScaleFrequency"), true, xScale == XScale::Frequency);
    yMenu.addItem (4, TRANS ("ScaleLinear"), true, yScale == YScale::Linear);
    yMenu.addItem (5, TRANS ("ScaleDecibel"), true, yScale == YScale::Decibel);
    menu.addSubMenu (TRANS ("XAxis"), xMenu);
    menu.addSubMenu (TRANS ("YAxis"), yMenu);
    menu.addItem (6, TRANS ("ToggleGrid"), true, showGrid);
    menu.addItem (7, TRANS ("ToggleInvertY"), true, invertY);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [this] (int res)
        {
            switch (res)
            {
                case 1: xScale = XScale::Samples; break;
                case 2: xScale = XScale::Time; break;
                case 3: xScale = XScale::Frequency; break;
                case 4: yScale = YScale::Linear; break;
                case 5: yScale = YScale::Decibel; break;
                case 6: showGrid = ! showGrid; break;
                case 7: invertY = ! invertY; break;
                default: break;
            }
            repaint();
        });
}
