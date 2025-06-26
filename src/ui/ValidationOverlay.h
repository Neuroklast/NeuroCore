#pragma once
#include <JuceHeader.h>
#include <thread>
#include <atomic>
#include "custom/ProgressBarComponent.h"
#include "custom/ModalOverlay.h"
class NeuroCoreAudioProcessor;

/** Modal overlay performing DSL validation and stability testing. */
class ValidationOverlay : public ui::ModalOverlay,
                          private juce::Timer
{
public:
    ValidationOverlay(NeuroCoreAudioProcessor& proc, const juce::String& expr);
    ~ValidationOverlay() override;

    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;

    std::function<void(bool)> onResult; //!< Called with true after validation finished. false indicates instability.

private:
    enum State { running, warning } state { running };
    NeuroCoreAudioProcessor& processor;
    juce::String script;
    std::unique_ptr<std::thread> worker;

    juce::String warningString;

    std::atomic<float> progress{ 0.0f };
    ui::ProgressBarComponent progressBar;

    std::atomic<bool> abortRequested{ false };

    juce::Label messageLabel;
    juce::TextButton okButton { "OK" };
    void startTest();
    void timerCallback() override;
    JUCE_DECLARE_WEAK_REFERENCEABLE(ValidationOverlay)
};
