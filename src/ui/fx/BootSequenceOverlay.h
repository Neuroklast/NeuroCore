#pragma once

#include <JuceHeader.h>
#include "CyberFxTypes.h"
#include "CyberSequence.h"

class BootSequenceOverlay : public juce::Component
{
public:
    BootSequenceOverlay();

    void setMotion (CyberMotion motion);
    void startOn (juce::Component& parent);
    void skip();

    std::function<void()> onFinished;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void onVBlank (double nowSec);
    void finish();

    CyberMotion motion { CyberMotion::Full };
    CyberSequence sequence;
    juce::VBlankAttachment vblank;
    juce::Random rng { 0x424f4f54 };
    double lastStamp { 0.0 };
    float  elapsed { 0.f };
    bool   finished { false };
};
