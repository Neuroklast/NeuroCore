#include <JuceHeader.h>
#include "LoudnessMeterComponent.h"
#include "../utils/Localiser.h"

LoudnessMeterComponent::LoudnessMeterComponent(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(30);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(true);
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.attachTo(*this);

    smoothedLoudness.reset(30.0, 0.1);
    smoothedLoudness.setCurrentAndTargetValue(-100.0f);
}

LoudnessMeterComponent::~LoudnessMeterComponent()
{
    openGLContext.detach();
}

void LoudnessMeterComponent::setCoveredByOverlay (bool covered)
{
    if (coveredByOverlay == covered)
        return;
    coveredByOverlay = covered;
    if (covered)
    {
        if (openGLContext.isAttached())
            openGLContext.detach();
        setVisible (false);
    }
    else
    {
        setVisible (true);
        if (! openGLContext.isAttached())
            openGLContext.attachTo (*this);
    }
}

void LoudnessMeterComponent::timerCallback()
{
    float db = processor.getLoudnessDb();
    // Hard-sanitize: NaN/Inf/out-of-range must never reach the bar or text
    if (! std::isfinite (db))
        db = -100.0f;
    db = juce::jlimit (-100.0f, 12.0f, db);
    smoothedLoudness.setTargetValue (db);
    limiter  = processor.isLimiterActive();

    // Red LED only for real DSP invalid events (debounced — no single-frame strobe)
    if (processor.consumeInvalidFlag())
        blinkCount = juce::jmax (blinkCount, 10); // ~330 ms at 30 Hz, non-retrigger spam

    if (blinkCount > 0)
    {
        // Solid-ish flash: on for 2 frames, off for 1 — less epileptic than toggle every tick
        blink = ((blinkCount / 2) % 2) == 0;
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
    // Invalid/NaN blink = signal red; idle = dark well
    g.setColour(on ? juce::Colour (0xffff1a1a) : juce::Colour (0xff2a0000));
    g.fillEllipse(area);
    if (on)
    {
        g.setColour (juce::Colour (0xffff1a1a).withAlpha (0.45f));
        g.drawEllipse (area.expanded (2.f), 1.5f);
    }
}

LoudnessMeterComponent::ScaleInfo LoudnessMeterComponent::currentScaleInfo() const noexcept
{
    switch (scale)
    {
    case Scale::LUFS:   return { "LUFS", -60.0f, 0.0f, 10.0f };
    case Scale::KSystem:return { "K-20", -20.0f, 20.0f, 5.0f };
    default:           return { "dBFS", -60.0f, 0.0f, 10.0f };
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
    glDisable(GL_DEBUG_OUTPUT);
}

void LoudnessMeterComponent::renderOpenGL()
{
    juce::OpenGLHelpers::clear(juce::Colours::black);
    auto bounds = getLocalBounds().toFloat();
    constexpr float labelWidth = Config::kLoudnessLabelWidth;
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

    // meter well (near-black)
    glColor3f(0.05f, 0.02f, 0.02f);
    glBegin(GL_QUADS);
    glVertex2f(meterArea.getX(), meterArea.getY());
    glVertex2f(meterArea.getRight(), meterArea.getY());
    glVertex2f(meterArea.getRight(), meterArea.getBottom());
    glVertex2f(meterArea.getX(), meterArea.getBottom());
    glEnd();

    // filled level — brand red; brighter when limiter engaged (real GR cue)
    loudness = smoothedLoudness.getNextValue();
    if (! std::isfinite (loudness))
        loudness = -100.0f;
    loudness = juce::jlimit (-100.0f, 12.0f, loudness);
    float yLevel = valueToY(loudness, meterArea);
    if (! std::isfinite (yLevel))
        yLevel = meterArea.getBottom();
    yLevel = juce::jlimit (meterArea.getY(), meterArea.getBottom(), yLevel);
    if (limiter)
        glColor3f(1.0f, 0.25f, 0.1f);
    else
        glColor3f(1.0f, 0.1f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(meterArea.getX(), yLevel);
    glVertex2f(meterArea.getRight(), yLevel);
    glVertex2f(meterArea.getRight(), meterArea.getBottom());
    glVertex2f(meterArea.getX(), meterArea.getBottom());
    glEnd();

    // grid lines (dim red)
    glColor3f(0.45f, 0.08f, 0.08f);
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
    // paint() must NOT advance the smoother again (OpenGL already did) — sample current
    loudness = smoothedLoudness.getCurrentValue();
    if (! std::isfinite (loudness))
        loudness = -100.0f;
    loudness = juce::jlimit (-100.0f, 12.0f, loudness);

    auto bounds = getLocalBounds().toFloat();
    constexpr float labelWidth = Config::kLoudnessLabelWidth;
    auto area = bounds.reduced(10.0f, 20.0f);
    auto labelArea = area.removeFromLeft(labelWidth);
    auto meterArea = area;
    auto info = currentScaleInfo();

    g.setColour(juce::Colour (0xffff1a1a).withAlpha (0.7f));
    g.drawRect(meterArea, 1.0f);

    for (float db = info.minDb; db <= info.maxDb; db += info.step)
    {
        float y = valueToY(db, meterArea);
        g.setColour(juce::Colour (0xff3a0000));
        g.drawLine(meterArea.getX(), y, meterArea.getRight(), y);
        g.setColour(juce::Colour (0xffc0c0c0));
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.f, juce::Font::plain));
        g.drawFittedText(juce::String(db, 0),
                         (int)labelArea.getX(),
                         (int)y - 7,
                         (int)labelArea.getWidth(),
                         14,
                         juce::Justification::centredRight,
                         1);
    }

    // LED = invalid (NaN/Inf) only. Limiter is shown via brighter bar fill, not red strobe.
    auto ledArea = juce::Rectangle<float>(bounds.getWidth() - 15.0f, 5.0f, 10.0f, 10.0f);
    drawLed(g, ledArea, blink);

    g.setColour(juce::Colour (0xffff1a1a));
    g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.f, juce::Font::bold));
    juce::String scaleName = info.name;
    if (limiter)
        scaleName += "  LIM";
    g.drawFittedText(scaleName, meterArea.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText(juce::String(loudness, 1) + " dB",
                     meterArea.toNearestInt().translated(0, -15),
                     juce::Justification::centredBottom, 1);
}

