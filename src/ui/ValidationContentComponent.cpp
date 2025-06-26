#include "ValidationContentComponent.h"
#include "../core/PluginProcessor.h"

ValidationContentComponent::ValidationContentComponent(NeuroCoreAudioProcessor& proc, const juce::String& expr)
    : processor(proc), script(expr)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(progressBar);
    addAndMakeVisible(messageLabel);
    addAndMakeVisible(okButton);
    okButton.onClick = [this]{ if(onResult) onResult(false); };
    okButton.setVisible(false);
    messageLabel.setJustificationType(juce::Justification::centred);
    messageLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    messageLabel.setText("Validating...", juce::dontSendNotification);

    startTimerHz(30);
    startTest();
}

ValidationContentComponent::~ValidationContentComponent()
{
    stopTimer();
    abortRequested = true;
    if (worker && worker->joinable())
        worker->join();
}

void ValidationContentComponent::startTest()
{
    auto text = script;
    worker = std::make_unique<std::thread>([this, text]() {
        juce::String warn;
        auto progressFn = [this](float p) {
            progress.store(p);
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
                messageLabel.setText(warn, juce::dontSendNotification);
                grabKeyboardFocus();
                resized();
                repaint();
            }
        });
    });
}

void ValidationContentComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(panel);
    g.setColour(juce::Colours::white);
    g.drawRect(panel);
}

void ValidationContentComponent::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth()*6/10, getHeight()*3/10);
    auto area = panel.reduced(8);
    auto barHeight = 24;
    progressBar.setBounds(area.removeFromTop(barHeight));
    area.removeFromTop(4);
    auto btnHeight = 24;
    okButton.setBounds(area.removeFromBottom(btnHeight).removeFromRight(80));
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
        progressBar.setProgress(progress.load());
}
