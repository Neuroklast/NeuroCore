#include "ValidationContentComponent.h"
#include "../core/PluginProcessor.h"

ValidationContentComponent::ValidationContentComponent(NeuroCoreAudioProcessor& proc, const juce::String& expr)
    : processor(proc), script(expr)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(progressBar);
    addAndMakeVisible(messageLabel);
    addAndMakeVisible(statsLabel);
    addAndMakeVisible(analysisLabel);
    addAndMakeVisible(icon);
    
    icon.setImage(juce::ImageCache::getFromMemory(BinaryData::warning_png,
                                                  BinaryData::warning_pngSize));
    icon.setVisible(false);
    
    processor.setValidationBypass(true);
    
    // Set up button callbacks
    okButton.onClick = [this]{ 
        if (state == State::success) {
            if (onResult) onResult(ValidationResult::Success);
        } else {
            if (onResult) onResult(ValidationResult::UserCancel);
        }
    };
    
    proceedButton.onClick = [this]{ 
        if (onResult) onResult(ValidationResult::UserProceed);
    };
    
    backToEditorButton.onClick = [this]{ 
        if (onResult) onResult(ValidationResult::UserCancel);
    };
    
    okButton.setVisible(false);
    proceedButton.setVisible(false);
    backToEditorButton.setVisible(false);
    
    messageLabel.setJustificationType(juce::Justification::centred);
    messageLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    messageLabel.setText("Validating...", juce::dontSendNotification);
    
    statsLabel.setJustificationType(juce::Justification::centred);
    statsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    analysisLabel.setJustificationType(juce::Justification::centred);
    analysisLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    analysisLabel.setFont(juce::Font(12.0f));

    startTimerHz(30);
    startTest();
}

ValidationContentComponent::~ValidationContentComponent()
{
    stopTimer();
    abortRequested = true;
    if (worker && worker->joinable())
        worker->join();
    processor.setValidationBypass(false);
}

void ValidationContentComponent::startTest()
{
    auto text = script;
    worker = std::make_unique<std::thread>([this, text]() {
        juce::String warn;
        auto progressFn = [this](const ValidationProgressInfo& info) {
            progress.store(info.progress);
            nanCount.store(info.nanCount);
            infCount.store(info.infCount);
            // Extended analysis for crackling/popping detection
            cracklePops.store(info.cracklePops);
            thd.store(info.thd);
            {
                const juce::SpinLock::ScopedLockType sl(textLock);
                progressText = info.message;
            }
            return !abortRequested.load();
        };
        
        bool ok = processor.testFormulaStability(text, warn, progressFn);
        if (abortRequested.load())
            return;
            
        juce::MessageManager::callAsync([cb = onResult, ok, warn, this]() mutable {
            if (ok)
            {
                state = State::success;
                progressBar.setVisible(false);
                okButton.setVisible(true);
                okButton.setButtonText("Apply");
                icon.setVisible(false);
                messageLabel.setText("Validation successful! No issues detected.", juce::dontSendNotification);
                
                // Show detailed analysis
                updateAnalysisDisplay();
                
                if (persistAfterSave) {
                    // Keep overlay visible after save for analysis review
                    messageLabel.setText("Code applied successfully. Analysis results:", juce::dontSendNotification);
                }
                
                grabKeyboardFocus();
                resized();
                repaint();
            }
            else
            {
                state = State::warning;
                warningString = warn;
                progressBar.setVisible(false);
                showWarningButtons(true);
                icon.setVisible(true);
                messageLabel.setText(warn, juce::dontSendNotification);
                
                // Show detailed analysis including issues found
                updateAnalysisDisplay();
                
                grabKeyboardFocus();
                resized();
                repaint();
            }
        });
    });
}

void ValidationContentComponent::paint(juce::Graphics& g)
{

}

void ValidationContentComponent::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth(), getHeight());
    auto area = panel.reduced(8);
    auto barHeight = 24;
    
    progressBar.setBounds(area.removeFromTop(barHeight));
    statsLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    
    auto btnHeight = 32;
    auto bottom = area.removeFromBottom(btnHeight);
    
    if (state == State::warning) {
        // Two buttons side by side for warning state
        auto buttonWidth = bottom.getWidth() / 2 - 4;
        proceedButton.setBounds(bottom.removeFromLeft(buttonWidth));
        bottom.removeFromLeft(8); // spacing
        backToEditorButton.setBounds(bottom.removeFromLeft(buttonWidth));
    } else {
        // Single OK button for success state
        okButton.setBounds(bottom.removeFromRight(80));
    }
    
    // Analysis label takes remaining space
    analysisLabel.setBounds(area.removeFromBottom(100));
    area.removeFromBottom(4);
    
    // Icon positioning
    icon.setBounds(getLocalBounds().removeFromTop(24).removeFromLeft(24).withTrimmedLeft(8));
    icon.setSize(getLocalBounds().getWidth(), getLocalBounds().getHeight());
    icon.toBack();
    icon.setAlpha(0.5f);
    
    // Message label takes remaining space
    messageLabel.setBounds(area);
}

bool ValidationContentComponent::keyPressed(const juce::KeyPress& kp)
{
    if (state == State::warning && (kp == juce::KeyPress::returnKey || kp == juce::KeyPress::escapeKey))
    {
        okButton.triggerClick();
        return true;
    }
    return true; // consume keys
}

void ValidationContentComponent::timerCallback()
{
    if (state == State::running)
    {
        progressBar.setProgress(progress.load());
        juce::String txt;
        {
            const juce::SpinLock::ScopedLockType sl(textLock);
            txt = progressText;
        }
        statsLabel.setText("NaN: " + juce::String(nanCount.load()) +
                           " Inf: " + juce::String(infCount.load()) +
                           " Pops: " + juce::String(cracklePops.load()),
                           juce::dontSendNotification);
        messageLabel.setText(txt, juce::dontSendNotification);
    }
}

void ValidationContentComponent::updateAnalysisDisplay()
{
    juce::String analysis;
    
    // Basic statistics
    analysis += "Sample Analysis:\n";
    analysis += "NaN values: " + juce::String(nanCount.load()) + "\n";
    analysis += "Inf values: " + juce::String(infCount.load()) + "\n";
    analysis += "Crackle/Pop events: " + juce::String(cracklePops.load()) + "\n";
    analysis += "THD estimate: " + juce::String(thd.load(), 3) + "%\n\n";
    
    // Performance analysis
    auto processingTime = processor.getLastProcessingTime();
    analysis += "Performance:\n";
    analysis += "Processing time: " + juce::String(processingTime, 2) + "ms\n";
    
    if (processingTime > 10.0) {
        analysis += "⚠ High processing time detected\n";
    }
    
    // Quality assessment
    analysis += "\nQuality Assessment:\n";
    if (nanCount.load() == 0 && infCount.load() == 0 && cracklePops.load() == 0) {
        analysis += "✓ Clean signal processing\n";
        analysis += "✓ No artifacts detected\n";
    } else {
        if (nanCount.load() > 0) analysis += "⚠ NaN values may cause dropouts\n";
        if (infCount.load() > 0) analysis += "⚠ Infinite values detected\n";
        if (cracklePops.load() > 0) analysis += "⚠ Audio artifacts detected\n";
    }
    
    analysisLabel.setText(analysis, juce::dontSendNotification);
}

void ValidationContentComponent::showWarningButtons(bool show)
{
    if (show) {
        addAndMakeVisible(proceedButton);
        addAndMakeVisible(backToEditorButton);
        okButton.setVisible(false);
        proceedButton.setButtonText("Proceed At Own Risk");
        backToEditorButton.setButtonText("Back to Editor");
    } else {
        proceedButton.setVisible(false);
        backToEditorButton.setVisible(false);
        okButton.setVisible(true);
    }
}
