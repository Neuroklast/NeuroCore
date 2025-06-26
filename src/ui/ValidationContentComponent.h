#pragma once
#include <JuceHeader.h>
#include <thread>
#include <atomic>
#include "custom/ProgressBarComponent.h"
class NeuroCoreAudioProcessor;

/** Content component performing validation with progress display. */
class ValidationContentComponent : public juce::Component, private juce::Timer
{
public:
    ValidationContentComponent(NeuroCoreAudioProcessor& proc, const juce::String& expr);
    ~ValidationContentComponent() override;

    std::function<void(bool)> onResult; // true if stable

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    enum class State { running, warning } state { State::running };
    NeuroCoreAudioProcessor& processor;
    juce::String script;
    std::unique_ptr<std::thread> worker;

    juce::String warningString;

    std::atomic<float> progress{0.0f};
    ui::ProgressBarComponent progressBar;

    std::atomic<bool> abortRequested{false};

    juce::Label messageLabel;
    juce::TextButton okButton{"OK"};
    juce::Rectangle<int> panel;

    void startTest();
    void timerCallback() override;
};
