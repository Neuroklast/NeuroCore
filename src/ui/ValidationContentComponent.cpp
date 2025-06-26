#include "ValidationContentComponent.h"
#include "../core/PluginProcessor.h"

ValidationContentComponent::ValidationContentComponent(NeuroCoreAudioProcessor& proc, const juce::String& expr)
    : processor(proc), script(expr)
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    addAndMakeVisible(progressBar);
    addAndMakeVisible(messageLabel);
    addAndMakeVisible(okButton);
    okButton.onClick = [this]
    {
        if (onResult)
            onResult(false);
    };
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
    auto scriptCopy = script;
    auto safeThis = juce::Component::SafePointer<ValidationContentComponent>(this);

    worker = std::make_unique<std::thread>([safeThis, scriptCopy, proc = std::ref(processor)]()
    {
        juce::String warn;
        const auto progressFn = [safeThis](float p)
        {
            if (safeThis)
            {
                safeThis->progress.store(p);
                return ! safeThis->abortRequested.load();
            }
            return false;
        };

        bool ok = false;
        if (! safeThis || safeThis->abortRequested.load())
            return;

        ok = proc.get().testFormulaStability(scriptCopy, warn, progressFn);

        if (! safeThis || safeThis->abortRequested.load())
            return;

        juce::MessageManager::callAsync([safeThis, ok, warn]()
        {
            if (! safeThis)
                return;

            if (ok)
            {
                if (safeThis->onResult)
                    safeThis->onResult(true);
            }
            else
            {
                safeThis->state = ValidationContentComponent::State::warning;
                safeThis->warningString = warn;
                safeThis->progressBar.setVisible(false);
                safeThis->okButton.setVisible(true);
                safeThis->messageLabel.setText(warn, juce::dontSendNotification);
                safeThis->grabKeyboardFocus();
                safeThis->resized();
                safeThis->repaint();
            }
        });
    });
}

void ValidationContentComponent::resized()
{
    auto area = getLocalBounds();
    auto barHeight = 24;
    progressBar.setBounds(area.removeFromTop(barHeight));
    area.removeFromTop(4);
    auto btnHeight = 24;
    okButton.setBounds(area.removeFromBottom(btnHeight).removeFromRight(80));
    messageLabel.setBounds(area);
}

bool ValidationContentComponent::keyPressed(const juce::KeyPress& kp)
{
    if (state == ValidationContentComponent::State::warning &&
        (kp == juce::KeyPress::returnKey || kp == juce::KeyPress::escapeKey))
    {
        okButton.triggerClick();
        return true;
    }
    return false;
}

void ValidationContentComponent::timerCallback()
{
    if (state == running)
    {
        progressBar.setProgress(progress.load());
    }
}
