#include "ValidationContentComponent.h"
#include "../core/PluginProcessor.h"

ValidationContentComponent::ValidationContentComponent(NeuroCoreAudioProcessor& proc, const juce::String& expr)
    : processor(proc), script(expr)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(progressBar);
    addAndMakeVisible(messageLabel);
    addAndMakeVisible(statsLabel);
    addAndMakeVisible(icon);
    icon.setImage(juce::ImageCache::getFromMemory(BinaryData::warning_png,
                                                  BinaryData::warning_pngSize));
    icon.setVisible(false);
    processor.setValidationBypass(true);
    okButton.onClick = [this]{ if(onResult) onResult(false); };
    okButton.setVisible(false);
    messageLabel.setJustificationType(juce::Justification::centred);
    messageLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    messageLabel.setText("Validating...", juce::dontSendNotification);
    statsLabel.setJustificationType(juce::Justification::centred);
    statsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	progressBar.setColours(juce::Colours::black, juce::Colours::red);

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
                if (cb) cb(true);
            }
            else
            {
                state = State::warning;
                warningString = warn;
                progressBar.setVisible(false);
                okButton.setVisible(true);
                icon.setVisible(true);
                messageLabel.setText(warn, juce::dontSendNotification);
                statsLabel.setText("NaN: " + juce::String(nanCount.load()) +
                                     " Inf: " + juce::String(infCount.load()),
                                     juce::dontSendNotification);
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
    auto btnHeight = 24;
    auto bottom = area.removeFromBottom(btnHeight);
    okButton.setBounds(bottom.removeFromRight(80));
    icon.setBounds(bottom.removeFromLeft(24));
	icon.setBounds(getLocalBounds().removeFromTop(24).removeFromLeft(24).withTrimmedLeft(8));
	icon.setSize(getLocalBounds().getWidth() , getLocalBounds().getHeight() );
	icon.toBack();
	icon.setAlpha(0.5f);
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
                           " Inf: " + juce::String(infCount.load()),
                           juce::dontSendNotification);
        messageLabel.setText(txt, juce::dontSendNotification);
    }
}
