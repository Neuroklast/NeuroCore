#include "LoudnessMeterComponent.h"

LoudnessMeterComponent::LoudnessMeterComponent(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(30);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(true);
    openGLContext.attachTo(*this);

    smoothedLoudness.reset(30.0, 0.1);
    smoothedLoudness.setCurrentAndTargetValue(-100.0f);
}

LoudnessMeterComponent::~LoudnessMeterComponent()
{
    openGLContext.detach();
}

void LoudnessMeterComponent::timerCallback()
{
    smoothedLoudness.setTargetValue(processor.getLoudnessDb());
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

LoudnessMeterComponent::ScaleInfo LoudnessMeterComponent::currentScaleInfo() const noexcept
{
    switch (scale)
    {
        case Scale::LUFS:   return { TRANS("MeterScaleLufs"), -60.0f, 0.0f, 10.0f };
        case Scale::KSystem:return { TRANS("MeterScaleKsys"), -20.0f, 20.0f, 5.0f };
        default:           return { TRANS("MeterScaleDbfs"), -60.0f, 0.0f, 10.0f };
    }
}

float LoudnessMeterComponent::valueToY(float db, juce::Rectangle<float> area) const noexcept
{
    auto info = currentScaleInfo();
    if (scale == Scale::KSystem)
        db += 20.0f; // convert dBFS -> K20
    float norm = juce::jlimit(0.0f, 1.0f, (db - info.minDb) / (info.maxDb - info.minDb));
    return area.getBottom() - norm * area.getHeight();
}

void LoudnessMeterComponent::showContextMenu()
{
    juce::PopupMenu m;
    m.addItem(1, TRANS("MeterScaleDbfs"), true, scale == Scale::dBFS);
    m.addItem(2, TRANS("MeterScaleLufs"), true, scale == Scale::LUFS);
    m.addItem(3, TRANS("MeterScaleKsys"), true, scale == Scale::KSystem);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this](int res)
    {
        switch (res)
        {
            case 1: scale = Scale::dBFS; break;
            case 2: scale = Scale::LUFS; break;
            case 3: scale = Scale::KSystem; break;
            default: return;
        }
        repaint();
    });
}

void LoudnessMeterComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
        showContextMenu();
}

void LoudnessMeterComponent::newOpenGLContextCreated()
{
    using namespace juce::gl;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, getWidth(), getHeight(), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void LoudnessMeterComponent::renderOpenGL()
{
    juce::OpenGLHelpers::clear(juce::Colours::black);
    auto bounds = getLocalBounds().toFloat();
    constexpr float labelWidth = 45.0f;
    auto area = bounds.reduced(10.0f, 20.0f);
    area.removeFromLeft(labelWidth);
    auto meterArea = area;
    auto info = currentScaleInfo();

    using namespace juce::gl;
    glViewport(0, 0, getWidth(), getHeight());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, getWidth(), getHeight(), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // meter background
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(meterArea.getX(), meterArea.getY());
    glVertex2f(meterArea.getRight(), meterArea.getY());
    glVertex2f(meterArea.getRight(), meterArea.getBottom());
    glVertex2f(meterArea.getX(), meterArea.getBottom());
    glEnd();

    // filled level
    loudness = smoothedLoudness.getNextValue();
    float yLevel = valueToY(loudness, meterArea);
    glColor3f(limiter ? 1.0f : 0.0f, limiter ? 0.0f : 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(meterArea.getX(), yLevel);
    glVertex2f(meterArea.getRight(), yLevel);
    glVertex2f(meterArea.getRight(), meterArea.getBottom());
    glVertex2f(meterArea.getX(), meterArea.getBottom());
    glEnd();

    // grid lines
    glColor3f(0.6f, 0.6f, 0.6f);
    glLineWidth(1.0f);
    for (float db = info.minDb; db <= info.maxDb; db += info.step)
    {
        float y = valueToY(db, meterArea);
        glBegin(GL_LINES);
        glVertex2f(meterArea.getX(), y);
        glVertex2f(meterArea.getRight(), y);
        glEnd();
    }

    // LED
    // all text and LED are drawn in paint()
}

void LoudnessMeterComponent::paint(juce::Graphics& g)
{
    loudness = smoothedLoudness.getNextValue();
    auto bounds = getLocalBounds().toFloat();
    constexpr float labelWidth = 45.0f;
    auto area = bounds.reduced(10.0f, 20.0f);
    auto labelArea = area.removeFromLeft(labelWidth);
    auto meterArea = area;
    auto info = currentScaleInfo();

    g.setColour(juce::Colours::white);
    g.drawRect(meterArea);

    for (float db = info.minDb; db <= info.maxDb; db += info.step)
    {
        float y = valueToY(db, meterArea);
        g.setColour(juce::Colours::darkgrey);
        g.drawLine(meterArea.getX(), y, meterArea.getRight(), y);
        g.setColour(juce::Colours::white);
        g.drawFittedText(juce::String(db, 0),
                         (int)labelArea.getX(),
                         (int)y - 7,
                         (int)labelArea.getWidth(),
                         14,
                         juce::Justification::centredRight,
                         1);
    }

    auto ledArea = juce::Rectangle<float>(bounds.getWidth() - 15.0f, 5.0f, 10.0f, 10.0f);
    drawLed(g, ledArea, blink);

    g.setColour(juce::Colours::white);
    g.drawFittedText(info.name, meterArea.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText(juce::String(loudness, 1) + " dB", meterArea.toNearestInt().translated(0, -15), juce::Justification::centredBottom, 1);
}

