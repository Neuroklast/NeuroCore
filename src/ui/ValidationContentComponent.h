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

    enum class ValidationResult { 
        Success,       // Validation passed, apply code
        UserProceed,   // Warning but user chose to proceed
        UserCancel,    // User chose to go back to editor
        Stable         // For compatibility - same as Success
    };
    
    std::function<void(ValidationResult)> onResult;
    
    void setPersistAfterSave(bool persist) { persistAfterSave = persist; }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    enum class State { running, warning, success, error } state { State::running };
    NeuroCoreAudioProcessor& processor;
    juce::String script;
    std::unique_ptr<std::thread> worker;

    juce::String warningString;
    juce::String analysisResults;

    std::atomic<float> progress{0.0f};
    std::atomic<int> nanCount{0};
    std::atomic<int> infCount{0};
    std::atomic<int> cracklePops{0};
    std::atomic<float> thd{0.0f};
    juce::SpinLock textLock;
    juce::String progressText;
    ui::ProgressBarComponent progressBar;
    juce::Label statsLabel;
    juce::Label analysisLabel;
    juce::ImageComponent icon;

    std::atomic<bool> abortRequested{false};
    bool persistAfterSave{false};

    juce::Label messageLabel;
    juce::TextButton okButton{"OK"};
    juce::TextButton proceedButton{"Proceed At Own Risk"};
    juce::TextButton backToEditorButton{"Back to Editor"};
    juce::Rectangle<int> panel;

    void startTest();
    void timerCallback() override;
    void updateAnalysisDisplay();
    void showWarningButtons(bool show);
};
