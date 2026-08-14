#pragma once

#include <JuceHeader.h>
#include "WaveformDisplayComponent.h"
#include "StereoFieldComponent.h"
#include "IoLoudnessMeter.h"

/** IN or OUT row: waveform + stereo field + loudness, extras can fold away. */
class ScopeDeck : public juce::Component
{
public:
    ScopeDeck (NeuroCoreAudioProcessor& proc, WaveformDisplayComponent::Type t);
    ~ScopeDeck() override = default;

    void resized() override;
    void paint (juce::Graphics& g) override;

    WaveformDisplayComponent& waveform() noexcept { return wave; }
    const WaveformDisplayComponent& waveform() const noexcept { return wave; }

    bool extrasOpen() const noexcept { return extrasVisible; }
    void setExtrasOpen (bool shouldOpen);
    void toggleExtras();

    std::function<void()> onExtrasChanged;

private:
    class FoldHit : public juce::Component,
                    public juce::SettableTooltipClient
    {
    public:
        explicit FoldHit (ScopeDeck& o) : owner (o)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setRepaintsOnMouseActivity (true);
        }

        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        ScopeDeck& owner;
    };

    WaveformDisplayComponent wave;
    StereoFieldComponent     field;
    IoLoudnessMeter          loud;
    FoldHit                  fold { *this };
    bool extrasVisible { true };
};
