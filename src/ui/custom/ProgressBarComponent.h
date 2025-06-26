#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace ui
{
    /** Simple thread-safe progress bar component. */
    class ProgressBarComponent : public juce::Component,
                                 private juce::Timer
    {
    public:
        ProgressBarComponent();
        ~ProgressBarComponent() override;

        /** Sets the progress value in range [0,1]. Thread-safe. */
        void setProgress(float newValue) noexcept;
        float getProgress() const noexcept;

        /** Customise bar and background colours. */
        void setColours(juce::Colour bar, juce::Colour background);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        std::atomic<float> progress { 0.0f };
        float progressDisplay { 0.0f };
        juce::Colour barColour { juce::Colours::deepskyblue };
        juce::Colour backgroundColour { juce::Colours::darkgrey };

        void timerCallback() override;
    };
} // namespace ui

