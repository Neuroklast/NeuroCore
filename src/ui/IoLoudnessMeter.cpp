#include "IoLoudnessMeter.h"
#include "PluginLookAndFeel.h"
#include "../dsp/DSPUtils.h"

IoLoudnessMeter::IoLoudnessMeter (NeuroKoreAudioProcessor& proc,
                                  WaveformDisplayComponent::Type t)
    : processor (proc), type (t)
{
    setOpaque (true);
    setTooltip ("Loudness for this side. Fill is average, tick is peak.");
    startTimerHz (Config::kMeterUiHz);
}

IoLoudnessMeter::~IoLoudnessMeter()
{
    stopTimer();
}

void IoLoudnessMeter::timerCallback()
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
    const auto s = ScopeAnalytics::analyse (L, R, n);

    const float dt = 1.f / (float) Config::kMeterUiHz;
    rmsDbL = DSPUtils::smoothMeterDb (rmsDbL, s.rmsDbL, dt,
                                      Config::kMeterUiAttackSec, Config::kMeterUiReleaseSec);
    rmsDbR = DSPUtils::smoothMeterDb (rmsDbR, s.rmsDbR, dt,
                                      Config::kMeterUiAttackSec, Config::kMeterUiReleaseSec);

    if (s.peakDbL >= peakDbL)
        peakDbL = s.peakDbL;
    else
        peakDbL = juce::jmax (s.peakDbL, peakDbL - dt * 24.f);

    if (s.peakDbR >= peakDbR)
        peakDbR = s.peakDbR;
    else
        peakDbR = juce::jmax (s.peakDbR, peakDbR - dt * 24.f);

    setTooltip (juce::String (type == WaveformDisplayComponent::Type::Input ? "IN  " : "OUT ")
                + "L " + juce::String (rmsDbL, 1) + "  R " + juce::String (rmsDbR, 1) + " dB");
    repaint();
}

void IoLoudnessMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> r,
                               float rmsDb, float peakDb, const char* tag) const
{
    const float cut = juce::jmin (4.f, r.getWidth() * 0.28f);
    juce::Path hull;
    hull.startNewSubPath (r.getX() + cut, r.getY());
    hull.lineTo (r.getRight(), r.getY());
    hull.lineTo (r.getRight(), r.getBottom() - cut);
    hull.lineTo (r.getRight() - cut, r.getBottom());
    hull.lineTo (r.getX(), r.getBottom());
    hull.lineTo (r.getX(), r.getY() + cut);
    hull.closeSubPath();

    g.setColour (NeuroKoreLookAndFeel::surfaceHigh());
    g.fillPath (hull);

    const auto dbToY = [r] (float db)
    {
        const float n = juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        return r.getBottom() - n * r.getHeight();
    };

    const float fillTop = dbToY (rmsDb);
    g.saveState();
    g.reduceClipRegion (hull);
    juce::ColourGradient fill (NeuroKoreLookAndFeel::accent(), r.getCentreX(), r.getY(),
                               NeuroKoreLookAndFeel::accentDim(), r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRect (r.getX(), fillTop, r.getWidth(), r.getBottom() - fillTop);

    const float peakY = dbToY (peakDb);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillRect (r.getX() + 1.f, peakY, r.getWidth() - 2.f, 1.4f);
    g.restoreState();

    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.55f));
    g.strokePath (hull, juce::PathStrokeType (1.f));

    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.80f));
    g.setFont (NeuroKoreLookAndFeel::monoFont (9.f));
    g.drawText (tag, r, juce::Justification::centredBottom, false);
}

void IoLoudnessMeter::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::black);

    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.80f));
    g.setFont (NeuroKoreLookAndFeel::monoFont (9.f));
    g.drawText (type == WaveformDisplayComponent::Type::Input ? "IN LU" : "OUT LU",
                full.removeFromTop (12.f), juce::Justification::centred, false);

    auto area = full.reduced (4.f, 3.f);
    auto left = area.removeFromLeft (area.getWidth() * 0.5f).reduced (1.5f, 0.f);
    auto right = area.reduced (1.5f, 0.f);
    drawBar (g, left, rmsDbL, peakDbL, "L");
    drawBar (g, right, rmsDbR, peakDbR, "R");
}
