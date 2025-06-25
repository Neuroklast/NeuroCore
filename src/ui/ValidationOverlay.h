#pragma once
#include <JuceHeader.h>
#include <thread>
class NeuroCoreAudioProcessor;

/** Modal overlay performing DSL validation and stability testing. */
class ValidationOverlay : public juce::Component,
                          private juce::Timer
{
public:
    ValidationOverlay(NeuroCoreAudioProcessor& proc, const juce::String& expr);
    ~ValidationOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;

    std::function<void(bool)> onResult; //!< Called with true after validation finished. false indicates instability.

private:
    enum State { running, warning } state { running };
    NeuroCoreAudioProcessor& processor;
    juce::String script;
    std::unique_ptr<std::thread> worker;

    juce::String warningString;

    double progress{ 0.0 };
    juce::ProgressBar progressBar{ progress };

    juce::Label messageLabel;
    juce::TextButton okButton { "OK" };
    juce::Rectangle<int> panel;

    void startTest();
    void timerCallback() override;
};
