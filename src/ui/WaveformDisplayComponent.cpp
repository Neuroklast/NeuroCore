#include "WaveformDisplayComponent.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <deque>
#include "../dsp/DSPUtils.h"
#include <juce_opengl/juce_opengl.h>
#if JUCE_WINDOWS
#include <GL/gl.h>
#include <GL/glu.h>
#elif JUCE_MAC
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

WaveformDisplayComponent::WaveformDisplayComponent(NeuroCoreAudioProcessor& proc, Type t)
    : processor(proc), type(t)
{
    auto& displays = juce::Desktop::getInstance().getDisplays();
    if (auto* display = displays.getPrimaryDisplay())
    {
        const int rate = display->verticalFrequencyHz.has_value()
                            ? static_cast<int>(*display->verticalFrequencyHz)
                            : 60;
        startTimerHz(rate);
    }
    else
    {
        startTimerHz(60);
    }

    setLookAndFeel(&lookAndFeel);

    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    openGLContext.setContinuousRepainting(true);
}

WaveformDisplayComponent::~WaveformDisplayComponent()
{
    openGLContext.detach();
    setLookAndFeel(nullptr);
}

void WaveformDisplayComponent::timerCallback()
{
    if (type == Type::Input)
        processor.getInputWaveform(buffer);
    else
        processor.getOutputWaveform(buffer);

    const auto* data = buffer.getReadPointer(0);
    const int num    = buffer.getNumSamples();
    int zeroIndex    = 0;

    for (int i = 1; i < num; ++i)
    {
        if (data[i - 1] < 0.0f && data[i] >= 0.0f)
        {
            zeroIndex = i;
            break;
        }
    }

    if (zeroIndex > 0)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* dst = buffer.getWritePointer(ch);
            std::rotate(dst, dst + zeroIndex, dst + num);
        }
    }

    if (xScale == XScale::Frequency)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        DSPUtils::analyseFFT(block, 0, fftMagnitudes, fftOrder);
    }

    if (showEcho)
    {
        std::vector<float> copy(buffer.getReadPointer(0),
                                buffer.getReadPointer(0) + buffer.getNumSamples());
        history.push_back(std::move(copy));
        while (static_cast<int>(history.size()) > Config::kWaveformEchoFrames)
            history.pop_front();
    }
    else
    {
        history.clear();
    }

    repaint();
}

float WaveformDisplayComponent::indexToX(int i, int total, juce::Rectangle<float> area) const
{
    auto norm = static_cast<float>(i) / static_cast<float>(juce::jmax(1, total - 1));
    return area.getX() + norm * area.getWidth();
}

float WaveformDisplayComponent::valueToY(float v, juce::Rectangle<float> area) const
{
    float scaled = juce::jlimit(-1.0f, 1.0f, v);
    if (yScale == YScale::Decibel)
    {
        const float db = 20.0f * std::log10(juce::jmax(1.0e-6f, std::abs(scaled)));
        scaled = juce::jlimit(-60.0f, 0.0f, db) / 60.0f;
        if (v < 0.0f)
            scaled = -scaled;
    }
    float norm = 0.5f - 0.5f * scaled;
    if (invertY)
        norm = 1.0f - norm;
    return area.getY() + norm * area.getHeight();
}

void WaveformDisplayComponent::updateTooltip(juce::Point<int> pos, juce::Rectangle<float> area)
{
    auto num = buffer.getNumSamples();
    int index = juce::jlimit(0, num - 1, static_cast<int>((pos.x - area.getX()) / area.getWidth() * num));
    float value = buffer.getSample(0, index);
    juce::String xStr;
    switch (xScale)
    {
        case XScale::Samples: xStr = juce::String(index); break;
        case XScale::Time:    xStr = juce::String((index / processor.getSampleRate()) * 1000.0, 2) + " " + TRANS("WaveUnitMs"); break;
        case XScale::Frequency:
            if (!fftMagnitudes.empty())
            {
                float freq = (index / static_cast<float>(fftMagnitudes.size())) * (processor.getSampleRate() / 2.0f);
                xStr = juce::String(freq, 1) + " Hz";
                value = fftMagnitudes[index];
            }
            break;
    }
    juce::String yStr;
    if (yScale == YScale::Decibel)
    {
        float db = 20.0f * std::log10(juce::jmax(1.0e-6f, std::abs(value)));
        yStr = juce::String(db, 1) + " dB";
    }
    else
    {
        yStr = juce::String(value, 2);
    }
    setTooltip(xStr + ", " + yStr);
}

void WaveformDisplayComponent::mouseMove(const juce::MouseEvent& e)
{
    auto area = getLocalBounds().toFloat()
                    .withTrimmedLeft(40.0f)
                    .withTrimmedRight(20.0f)
                    .withTrimmedTop(20.0f)
                    .withTrimmedBottom(40.0f);
    updateTooltip(e.position.toInt(), area);
}

void WaveformDisplayComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        juce::PopupMenu menu;
        juce::PopupMenu xMenu, yMenu;
        xMenu.addItem(1, TRANS("WaveMenuSamples"), true, xScale == XScale::Samples);
        xMenu.addItem(2, TRANS("WaveMenuTime"), true, xScale == XScale::Time);
        xMenu.addItem(3, TRANS("WaveMenuFrequency"), true, xScale == XScale::Frequency);
        yMenu.addItem(4, TRANS("WaveMenuLinear"), true, yScale == YScale::Linear);
        yMenu.addItem(5, TRANS("WaveMenuDecibel"), true, yScale == YScale::Decibel);
        menu.addSubMenu(TRANS("WaveAxisX"), xMenu);
        menu.addSubMenu(TRANS("WaveAxisY"), yMenu);
        menu.addItem(6, TRANS("WaveMenuGrid"), true, showGrid);
        menu.addItem(7, TRANS("WaveMenuInvertY"), true, invertY);
        menu.addItem(8, TRANS("WaveMenuEcho"), true, showEcho);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this](int res)
            {
                switch (res)
                {
                    case 1: xScale = XScale::Samples; break;
                    case 2: xScale = XScale::Time; break;
                    case 3: xScale = XScale::Frequency; break;
                    case 4: yScale = YScale::Linear; break;
                    case 5: yScale = YScale::Decibel; break;
                    case 6: showGrid = !showGrid; break;
                    case 7: invertY = !invertY; break;
                    case 8: showEcho = !showEcho; break;
                    default: break;
                }
                repaint();
            });
    }
}

void WaveformDisplayComponent::newOpenGLContextCreated()
{
    juce::OpenGLHelpers::clear(lookAndFeel.findColour(WaveformLookAndFeel::backgroundColourId));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, getWidth(), getHeight(), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glLineWidth(lineWidth);
    glColor4f(waveColour.getFloatRed(), waveColour.getFloatGreen(), waveColour.getFloatBlue(), 0.7f);
    auto drawBuffer = [&](const float* bufferData, int count, float alpha)
        glColor4f(waveColour.getFloatRed(), waveColour.getFloatGreen(), waveColour.getFloatBlue(), alpha);
        glLineWidth(lineWidth);
        for (int i = 0; i < count; ++i)
            float value = (xScale == XScale::Frequency && !fftMagnitudes.empty()) ? fftMagnitudes[i] : bufferData[i];
            float x = indexToX(i, count, area);
    };

    drawBuffer(data, num, 1.0f);

    if (showEcho)
    {
        float alphaStep = 1.0f / (static_cast<float>(history.size()) + 1.0f);
        float a = alphaStep;
        for (auto it = history.rbegin(); it != history.rend(); ++it, a += alphaStep)
        {
            int count = static_cast<int>(it->size());
            drawBuffer(it->data(), count, juce::jlimit(0.0f, 1.0f, a));
        }
                    .withTrimmedLeft(40.0f)
                    .withTrimmedRight(20.0f)
                    .withTrimmedTop(20.0f)
                    .withTrimmedBottom(40.0f);

    g.setColour(lookAndFeel.findColour(WaveformLookAndFeel::axisColourId));
            g.setColour(lookAndFeel.findColour(WaveformLookAndFeel::gridColourId).withAlpha(alpha));
        g.setColour(lookAndFeel.findColour(WaveformLookAndFeel::axisColourId));
        else if (xScale == XScale::Time) label = juce::String(value, 0) + " " + TRANS("WaveUnitMs");
        else label = juce::String(value, 0) + " " + TRANS("WaveUnitHz");
            g.setColour(lookAndFeel.findColour(WaveformLookAndFeel::gridColourId).withAlpha(0.5f));
        g.setColour(lookAndFeel.findColour(WaveformLookAndFeel::axisColourId));
            g.drawFittedText(juce::String(value, 0) + " " + TRANS("WaveUnitDb"), (int)plot.getX() - 50, (int)y - 10, 45, 20, juce::Justification::centredRight, 1);
    g.drawFittedText(xScale == XScale::Samples ? TRANS("WaveAxisSamples") : (xScale == XScale::Time ? TRANS("WaveAxisTime") : TRANS("WaveAxisFrequency")),
                    (int)plot.getX(), (int)plot.getBottom() + 26, (int)plot.getWidth(), 20, juce::Justification::centred, 1);
    g.drawFittedText(yScale == YScale::Linear ? TRANS("WaveAxisAmplitude") : TRANS("WaveAxisAmplitudeDb"),
                    (int)plot.getX() - 80, (int)plot.getY(), 60, (int)plot.getHeight(), juce::Justification::centredRight, 1);
    for (int pass = 3; pass >= 0; --pass)
    {
        float alpha = 0.2f + 0.2f * pass;
        glLineWidth(1.5f + static_cast<float>(pass));
        glColor4f(glowColour.getFloatRed(), glowColour.getFloatGreen(), glowColour.getFloatBlue(), alpha);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < num; ++i)
        {
            float value = (xScale == XScale::Frequency && !fftMagnitudes.empty()) ? fftMagnitudes[i] : data[i];
            float x = indexToX(i, num, area);
            float y = valueToY(value, area);
            glVertex2f(x, y);
        }
        glEnd();
    }
}

void WaveformDisplayComponent::drawAxes(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto plot = bounds
                    .withTrimmedLeft(40.0f)
                    .withTrimmedRight(20.0f)
                    .withTrimmedTop(20.0f)
                    .withTrimmedBottom(40.0f);

    g.setColour(juce::Colours::white);
    g.drawRect(plot);

    int xTicks = 10;
    int yTicks = 4;

    for (int i = 0; i <= xTicks; ++i)
    {
        float alpha = i == 0 || i == xTicks ? 1.0f : 0.5f;
        float x = plot.getX() + (plot.getWidth() / xTicks) * i;
        if (showGrid)
        {
            g.setColour(juce::Colours::darkgrey.withAlpha(alpha));
            g.drawLine(x, plot.getY(), x, plot.getBottom(), 0.5f);
        }
        g.setColour(juce::Colours::white);
        g.drawLine(x, plot.getBottom(), x, plot.getBottom() + 4.0f);
        double value = 0.0;
        switch (xScale)
        {
            case XScale::Samples: value = juce::jmap(static_cast<double>(i), 0.0, static_cast<double>(xTicks), 0.0, static_cast<double>(buffer.getNumSamples() - 1)); break;
            case XScale::Time: value = juce::jmap(static_cast<double>(i), 0.0, static_cast<double>(xTicks), 0.0, (buffer.getNumSamples() - 1) / processor.getSampleRate()); value *= 1000.0; break;
            case XScale::Frequency: value = juce::jmap(static_cast<double>(i), 0.0, static_cast<double>(xTicks), 0.0, processor.getSampleRate() / 2.0); break;
        }
        juce::String label;
        if (xScale == XScale::Samples) label = juce::String(static_cast<int>(value));
        else if (xScale == XScale::Time) label = juce::String(value, 0) + " ms";
        else label = juce::String(value, 0) + " Hz";
        g.drawFittedText(label, (int)x - 30, (int)plot.getBottom() + 6, 60, 20, juce::Justification::centred, 1);
    }

    for (int i = 0; i <= yTicks; ++i)
    {
        float y = plot.getY() + (plot.getHeight() / yTicks) * i;
        if (showGrid)
        {
            g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));
            g.drawLine(plot.getX(), y, plot.getRight(), y, 0.5f);
        }
        g.setColour(juce::Colours::white);
        g.drawLine(plot.getX() - 4.0f, y, plot.getX(), y);
        double value = juce::jmap(static_cast<double>(i), 0.0, static_cast<double>(yTicks), 1.0, -1.0);
        if (yScale == YScale::Decibel)
        {
            value *= 60.0;
            g.drawFittedText(juce::String(value, 0) + " dB", (int)plot.getX() - 50, (int)y - 10, 45, 20, juce::Justification::centredRight, 1);
        }
        else
        {
            g.drawFittedText(juce::String(value, 2), (int)plot.getX() - 50, (int)y - 10, 45, 20, juce::Justification::centredRight, 1);
        }
    }

    g.drawFittedText(xScale == XScale::Samples ? "Samples" : (xScale == XScale::Time ? "Zeit (ms)" : "Frequenz (Hz)"), (int)plot.getX(), (int)plot.getBottom() + 26, (int)plot.getWidth(), 20, juce::Justification::centred, 1);
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, plot.getX() - 30.0f, plot.getCentreY()));
    g.drawFittedText(yScale == YScale::Linear ? "Amplitude" : "Amplitude (dB)", (int)plot.getX() - 80, (int)plot.getY(), 60, (int)plot.getHeight(), juce::Justification::centredRight, 1);
}

void WaveformDisplayComponent::paint(juce::Graphics& g)
{
    drawAxes(g);
}
