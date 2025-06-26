#include <JuceHeader.h>
#include "WaveformDisplayComponent.h"
#include <algorithm>
#include "../dsp/DSPUtils.h"
#include "../utils/Localiser.h"
#include <juce_opengl/juce_opengl.h>
#if JUCE_WINDOWS
#include <GL/gl.h>
#elif JUCE_MAC
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

WaveformDisplayComponent::WaveformDisplayComponent(NeuroCoreAudioProcessor& proc, Type t)
    : processor(proc), type(t)
{
    setOpaque(false);
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

    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    openGLContext.setContinuousRepainting(true);
    

    const double rate = 60.0;
    smoothedData.resize(buffer.getNumSamples());
    for (auto& s : smoothedData)
        s.reset(rate, 0.1);
    smoothedFft.resize(static_cast<size_t>(1u << (fftOrder - 1)));
    for (auto& s : smoothedFft)
        s.reset(rate, 0.1);
}

WaveformDisplayComponent::~WaveformDisplayComponent()
{
    openGLContext.detach();
}

void WaveformDisplayComponent::timerCallback()
{
    if (type == Type::Input)
        processor.getInputWaveform(buffer);
    else
        processor.getOutputWaveform(buffer);

    const auto* data = buffer.getReadPointer(0);
    const int num    = buffer.getNumSamples();

    if (fixedWave)
    {
        int zeroIndex = 0;
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
    }

    for (int i = 0; i < num; ++i)
        smoothedData[(size_t)i].setTargetValue(buffer.getSample(0, i));

    if (xScale == XScale::Frequency)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        DSPUtils::analyseFFT(block, 0, fftMagnitudes, fftOrder);
        if (smoothedFft.size() != fftMagnitudes.size())
        {
            smoothedFft.resize(fftMagnitudes.size());
            const double rate = 60.0;
            for (auto& s : smoothedFft)
                s.reset(rate, 0.1);
        }
        for (size_t i = 0; i < fftMagnitudes.size(); ++i)
            smoothedFft[i].setTargetValue(fftMagnitudes[i]);
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

void WaveformDisplayComponent::updateTooltip(juce::Point<int> pos,
    juce::Rectangle<float> area)
{
    // 1) Total Samples bzw. FFT-Bins ermitteln
    const int total = (xScale == XScale::Frequency && !fftMagnitudes.empty())
        ? static_cast<int>(fftMagnitudes.size())
        : buffer.getNumSamples();

    // 2) Wie viele Punkte angezeigt werden
    const int num = juce::jlimit(1, total, static_cast<int>(total / zoom));

    // 3) Index anhand der Maus-X-Position, geklammert auf [0..num-1]
    const int index = juce::jlimit(0, num - 1,
        static_cast<int>((pos.x - area.getX())
            / area.getWidth()
            * num));

    // 4) Wert aus dem richtigen Glätter-Array
    float value;
    if (xScale == XScale::Frequency && !smoothedFft.empty())
        value = smoothedFft[(size_t)index].getCurrentValue();
    else
        value = smoothedData[(size_t)index].getCurrentValue();

    // 5) X-Label
    juce::String xStr;
    if (xScale == XScale::Frequency)
    {
        float freq = (index / static_cast<float>(fftMagnitudes.size()))
            * (processor.getSampleRate() / 2.0f);
        xStr = juce::String(freq, 1) + " Hz";
    }
    else if (xScale == XScale::Time)
    {
        xStr = juce::String((index / processor.getSampleRate()) * 1000.0, 2)
            + " ms";
    }
    else  // Samples
    {
        xStr = juce::String(index);
    }

    // 6) Y-Label (linear vs. Dezibel)
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
        xMenu.addItem(1, TRANS("ScaleSamples"), true, xScale == XScale::Samples);
        xMenu.addItem(2, TRANS("ScaleTime"), true, xScale == XScale::Time);
        xMenu.addItem(3, TRANS("ScaleFrequency"), true, xScale == XScale::Frequency);
        yMenu.addItem(4, TRANS("ScaleLinear"), true, yScale == YScale::Linear);
        yMenu.addItem(5, TRANS("ScaleDecibel"), true, yScale == YScale::Decibel);
        menu.addSubMenu(TRANS("XAxis"), xMenu);
        menu.addSubMenu(TRANS("YAxis"), yMenu);
        menu.addItem(6, TRANS("ToggleGrid"), true, showGrid);
        menu.addItem(7, TRANS("ToggleInvertY"), true, invertY);
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
                    default: break;
                }
                repaint();
            });
    }
}

void WaveformDisplayComponent::newOpenGLContextCreated()
{
	using namespace juce::gl;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, getWidth(), getHeight(), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEBUG_OUTPUT);


    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void WaveformDisplayComponent::renderOpenGL()
{
    juce::OpenGLHelpers::clear(juce::Colours::transparentBlack);
    auto area = getLocalBounds().toFloat()
                    .withTrimmedLeft(60.0f)
                    .withTrimmedRight(20.0f)
                    .withTrimmedTop(20.0f)
                    .withTrimmedBottom(50.0f);

    auto scale = openGLContext.getRenderingScale();
    using namespace juce::gl;
    glViewport(0, 0, juce::roundToInt(getWidth() * scale), juce::roundToInt(getHeight() * scale));

    glLineWidth(lineThickness);
    glColor4f(lineColour.getFloatRed(), lineColour.getFloatGreen(), lineColour.getFloatBlue(), 0.7f);

    int total = (xScale == XScale::Frequency && !fftMagnitudes.empty()) ? static_cast<int>(fftMagnitudes.size()) : buffer.getNumSamples();
    int num   = juce::jlimit(1, total, static_cast<int>(total / zoom));

    for (int pass = 1; pass >= 0; --pass)
    {
        float alpha = 0.2f + 0.2f * pass;
        glLineWidth(lineThickness + static_cast<float>(pass));
        glColor4f(lineColour.getFloatRed(), lineColour.getFloatGreen(), lineColour.getFloatBlue(), alpha);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < num; ++i)
        {
            float value = 0.0f;
            if (xScale == XScale::Frequency && !fftMagnitudes.empty())
                value = smoothedFft[(size_t)i].getNextValue();
            else
                value = smoothedData[(size_t)i].getNextValue();

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
                    .withTrimmedLeft(60.0f)
                    .withTrimmedRight(20.0f)
                    .withTrimmedTop(20.0f)
                    .withTrimmedBottom(50.0f);

    g.setColour(juce::Colours::white);
    g.drawRect(plot);

    int xTicks = 10;
    int yTicks = 4;
    int totalSamples = buffer.getNumSamples();
    int visibleSamples = juce::jlimit(1, totalSamples, static_cast<int>(totalSamples / zoom));

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
            case XScale::Samples: value = juce::jmap(static_cast<double>(i), 0.0, static_cast<double>(xTicks), 0.0, static_cast<double>(visibleSamples - 1)); break;
            case XScale::Time: value = juce::jmap(static_cast<double>(i), 0.0, static_cast<double>(xTicks), 0.0, (visibleSamples - 1) / processor.getSampleRate()); value *= 1000.0; break;
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

    g.drawFittedText(xScale == XScale::Samples ? TRANS("ScaleSamples") : (xScale == XScale::Time ? TRANS("ScaleTime") : TRANS("ScaleFrequency")),
                    (int)plot.getX(), (int)plot.getBottom() + 26,
                    (int)plot.getWidth(), 20,
                    juce::Justification::centred, 1);
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                   plot.getX() - 30.0f,
                                                   plot.getCentreY()));
    g.drawFittedText(yScale == YScale::Linear ? TRANS("AxisAmplitude") : TRANS("AxisAmplitudeDb"),
                    (int)plot.getX() - 70, (int)plot.getY() - 15,
                    60, (int)plot.getHeight(), juce::Justification::centredRight, 1);
}

void WaveformDisplayComponent::paint(juce::Graphics& g)
{
    drawAxes(g);
}
