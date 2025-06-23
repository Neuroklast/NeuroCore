#pragma once
#include <JuceHeader.h>
#include "../core/Config.h"

// Look and feel for WaveformDisplayComponent.
class WaveformLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum ColourIds
    {
        backgroundColourId = 0x2351000,
        waveformColourId,
        gridColourId,
        axisColourId,
        echoColourId
    };

    WaveformLookAndFeel()
    {
        using namespace juce;
        setColour(backgroundColourId, Colours::black);
        setColour(waveformColourId, Colour(Config::kWaveformColourARGB));
        setColour(gridColourId, Colours::darkgrey);
        setColour(axisColourId, Colours::white);
        setColour(echoColourId,
                  Colour(Config::kWaveformColourARGB).withAlpha(0.3f));
    }

    ~WaveformLookAndFeel() override = default;
};
