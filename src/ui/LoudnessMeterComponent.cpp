#include <JuceHeader.h>
#include <cmath>
#include "LoudnessMeterComponent.h"
#include "PluginLookAndFeel.h"
#include "../dsp/DSPUtils.h"
#include "../utils/Localiser.h"

LoudnessMeterComponent::LoudnessMeterComponent(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz (Config::kMeterUiHz);
}

void LoudnessMeterComponent::setCoveredByOverlay (bool covered)
{
    if (coveredByOverlay == covered)
        return;
    coveredByOverlay = covered;
    setVisible (! covered);
}

void LoudnessMeterComponent::timerCallback()
{
    float db = processor.getLoudnessDb();
    if (! std::isfinite (db))
        db = -100.0f;
    db = juce::jlimit (-100.0f, 12.0f, db);
    loudness = DSPUtils::smoothMeterDb (loudness, db,
                                        1.0f / (float) Config::kMeterUiHz,
                                        Config::kMeterUiAttackSec,
                                        Config::kMeterUiReleaseSec);
    limiter  = processor.isLimiterActive();

    if (processor.consumeInvalidFlag())
        blinkCount = juce::jmax (blinkCount, 10);

    if (blinkCount > 0)
    {
        blink = ((blinkCount / 2) % 2) == 0;
        --blinkCount;
    }
    else
    {
        blink = false;
    }

    const float fillNorm = juce::jlimit (0.f, 1.f, (loudness + 60.f) / 60.f);
    if (fillNorm > 0.08f && rng.nextFloat() < 0.25f + 0.45f * fillNorm)
        glitchSeed = rng.nextInt();

    repaint();
}

void LoudnessMeterComponent::drawLed(juce::Graphics& g,
                                     juce::Rectangle<float> area, bool on)
{
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
        db += 20.0f;
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

int LoudnessMeterComponent::bandHeightPx (float fillNorm, float heightFromBottom01) noexcept
{
    const float h = juce::jlimit (0.f, 1.f, heightFromBottom01);
    const float f = juce::jlimit (0.f, 1.f, fillNorm);
    const float intensity = std::pow (h, 1.35f) * (0.25f + 0.75f * f);
    return juce::jmax (2, (int) std::lround (2.f + intensity * 7.f));
}

void LoudnessMeterComponent::drawOverloadFill (juce::Graphics& g,
                                               juce::Rectangle<float> meterArea,
                                               float fillTop,
                                               float fillNorm) noexcept
{
    const juce::Colour topC = limiter ? juce::Colour (0xffff4a22) : NeuroCoreLookAndFeel::accent();
    const juce::Colour botC = limiter ? juce::Colour (0xffff1a1a)
                                      : NeuroCoreLookAndFeel::accentDim();
    const float fill = juce::jlimit (0.f, 1.f, fillNorm);

    juce::Random px (glitchSeed);
    float y = meterArea.getBottom();
    const float hMeter = juce::jmax (1.f, meterArea.getHeight());

    while (y > fillTop + 0.4f)
    {
        const float height01 = (meterArea.getBottom() - y) / hMeter;
        const float intensity = std::pow (juce::jlimit (0.f, 1.f, height01), 1.35f)
                              * (0.25f + 0.75f * fill);
        const int band = bandHeightPx (fillNorm, height01);
        const float y0 = juce::jmax (fillTop, y - (float) band);
        const float h = y - y0;
        const auto c = botC.interpolatedWith (topC, juce::jlimit (0.f, 1.f, height01));

        const int cols = 1 + (int) std::lround (intensity * 6.f);
        const float cw = meterArea.getWidth() / (float) juce::jmax (1, cols);
        const float gap = intensity * 0.9f;
        for (int col = 0; col < cols; ++col)
        {
            if (px.nextFloat() < 0.06f * intensity)
                continue;
            const float bri = 1.f - intensity * 0.08f + px.nextFloat() * 0.22f * intensity;
            g.setColour (c.withMultipliedBrightness (bri));
            g.fillRect (meterArea.getX() + (float) col * cw + gap * 0.5f, y0,
                        juce::jmax (1.f, cw - gap), h);
        }
        y = y0;
    }

    const float sliceAmt = std::pow (fill, 1.2f);
    if (sliceAmt > 0.04f)
    {
        const int n = (int) std::lround (sliceAmt * 5.f);
        for (int i = 0; i < n; ++i)
        {
            const float span = juce::jmax (4.f, meterArea.getBottom() - fillTop);
            const float ySlice = fillTop + px.nextFloat() * span;
            const float sh = 1.f + px.nextFloat() * (1.2f + 3.2f * sliceAmt);
            const float dx = (px.nextFloat() - 0.5f) * 8.f * sliceAmt;
            const auto sliceC = (i % 3 == 0 ? juce::Colours::cyan
                                            : (i % 3 == 1 ? juce::Colours::magenta
                                                          : NeuroCoreLookAndFeel::accent()));
            g.setColour (sliceC.withAlpha (0.08f + 0.32f * sliceAmt));
            g.fillRect (meterArea.getX() + dx, ySlice, meterArea.getWidth(), sh);
        }
    }
}

juce::Path LoudnessMeterComponent::makeHull (juce::Rectangle<float> r) const
{
    const float cut = juce::jmin (7.f, r.getWidth() * 0.28f);
    juce::Path hull;
    hull.startNewSubPath (r.getX() + cut, r.getY());
    hull.lineTo (r.getRight(), r.getY());
    hull.lineTo (r.getRight(), r.getBottom() - cut);
    hull.lineTo (r.getRight() - cut, r.getBottom());
    hull.lineTo (r.getX(), r.getBottom());
    hull.lineTo (r.getX(), r.getY() + cut);
    hull.closeSubPath();
    return hull;
}

void LoudnessMeterComponent::paint(juce::Graphics& g)
{
    float shown = loudness;
    if (! std::isfinite (shown))
        shown = -100.0f;
    shown = juce::jlimit (-100.0f, 12.0f, shown);

    auto bounds = getLocalBounds().toFloat();
    constexpr float labelWidth = Config::kLoudnessLabelWidth;
    auto area = bounds.reduced(8.0f, 16.0f);
    auto labelArea = area.removeFromLeft(labelWidth);
    auto meterArea = area.reduced (2.f, 0.f);
    auto info = currentScaleInfo();

    auto hull = makeHull (meterArea);
    g.setColour (juce::Colours::black);
    g.fillPath (hull);
    g.setColour (NeuroCoreLookAndFeel::surfaceHigh());
    g.fillPath (hull);

    const float yLevel = valueToY (shown, meterArea);
    const float fillTop = juce::jlimit (meterArea.getY(), meterArea.getBottom(), yLevel);
    const float shownForNorm = (scale == Scale::KSystem) ? shown + 20.f : shown;
    const float fillNorm = juce::jlimit (0.f, 1.f,
                                         (shownForNorm - info.minDb) / (info.maxDb - info.minDb));

    g.saveState();
    g.reduceClipRegion (hull);
    drawOverloadFill (g, meterArea, fillTop, fillNorm);

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.18f));
    for (int i = 1; i < 10; ++i)
    {
        const float y = meterArea.getY() + meterArea.getHeight() * (float) i / 10.f;
        g.fillRect (meterArea.getX() + 2.f, y, meterArea.getWidth() - 4.f, 1.f);
    }
    g.restoreState();

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f));
    g.strokePath (hull, juce::PathStrokeType (1.2f));

    g.setFont (NeuroCoreLookAndFeel::monoFont (11.f));
    for (float db = info.minDb; db <= info.maxDb; db += info.step)
    {
        float y = valueToY(db, meterArea);
        g.setColour (NeuroCoreLookAndFeel::mutedText());
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

    g.setColour (NeuroCoreLookAndFeel::accent());
    g.setFont (NeuroCoreLookAndFeel::monoFont (11.f));
    juce::String scaleName = info.name;
    if (limiter)
        scaleName += "  LIM";
    g.drawFittedText(scaleName, meterArea.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText(juce::String(shown, 1) + " dB",
                     meterArea.toNearestInt().translated(0, -15),
                     juce::Justification::centredBottom, 1);
}
