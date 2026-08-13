#pragma once
#include <JuceHeader.h>
#include "../../core/EffectParameters.h"

/** Three-position input selector: Left / Both / Right. */
class InputChannelSwitch : public juce::Component,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit InputChannelSwitch (juce::AudioProcessorValueTreeState& state);
    ~InputChannelSwitch() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    EffectParameters::InputChannelMode getMode() const noexcept { return mode; }

private:
    void parameterChanged (const juce::String&, float) override;
    void syncFromParams();
    void applyMode (EffectParameters::InputChannelMode m);
    EffectParameters::InputChannelMode modeAt (float x) const noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    EffectParameters::InputChannelMode mode { EffectParameters::InputChannelMode::Both };
    bool applying { false };
};
