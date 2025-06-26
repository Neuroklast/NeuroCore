#pragma once
#include <JuceHeader.h>
#include <thread>
#include <atomic>
#include "custom/ProgressBarComponent.h"
class NeuroCoreAudioProcessor;

/** Component performing DSL validation and stability testing. */
class ValidationContentComponent : public juce::Component,
                                  private juce::Timer
{
public:
    ValidationContentComponent(NeuroCoreAudioProcessor& proc, const juce::String& expr);
    ~ValidationContentComponent() override;

    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;

    std::function<void(bool)> onResult; //!< true if validation successful

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
};
