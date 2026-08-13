#pragma once

#include <JuceHeader.h>
#include "CyberBackdropCache.h"
#include "CyberFxDirector.h"

class CyberBackdropComponent : public juce::Component
{
public:
    explicit CyberBackdropComponent (CyberFxDirector& directorIn);

    void setPeakFromDb (float loudnessDb);
    void advance (float dtSec);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    CyberFxDirector& director;
    CyberBackdropCache cache;
    juce::Random rng { 0x43594252 };
};
