#include "ValidationOverlay.h"
#include "../core/PluginProcessor.h"

ValidationOverlay::ValidationOverlay(NeuroCoreAudioProcessor& proc, const juce::String& expr)
    : ModalOverlay(false), processor(proc), script(expr)
{

    addAndMakeVisible(progressBar);
    addAndMakeVisible(messageLabel);
    addAndMakeVisible(okButton);
    okButton.onClick = [this] {
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

ValidationOverlay::~ValidationOverlay()
{
    stopTimer();
    abortRequested = true;
    if (worker && worker->joinable())
        worker->join();
}

void ValidationOverlay::startTest()
{

    // Copy script to avoid capturing this after component destruction
    auto scriptCopy = script;

    auto weakThis = juce::WeakReference<ValidationOverlay>(this);

    // Start worker thread that validates the formula asynchronously
    worker = std::make_unique<std::thread>([weakThis, scriptCopy, proc = std::ref(processor)]()
    {
        juce::String warn;
        const auto progressFn = [weakThis](float p)
        {
            if (auto* self = weakThis.get())
            {
                self->progress.store(p);
                return ! self->abortRequested.load();
            }
            return false;
        };

        bool ok = false;
        if (weakThis == nullptr || weakThis->abortRequested.load())
            return;

        ok = proc.get().testFormulaStability(scriptCopy, warn, progressFn);

        if (weakThis == nullptr || weakThis->abortRequested.load())
            return;

        juce::MessageManager::callAsync([weakThis, ok, warn]()
        {
            if (auto* self = weakThis.get())
            {
                if (ok)
                {
                    if (self->onResult)
                        self->onResult(true);
                }
                else
                {
                    self->state = ValidationOverlay::State::warning;
                    self->warningString = warn;
                    self->progressBar.setVisible(false);
                    self->okButton.setVisible(true);
                    self->messageLabel.setText(warn, juce::dontSendNotification);
                    self->grabKeyboardFocus();
                    self->resized();
                    self->repaint();
                }
            }
        });
    });
}

void ValidationOverlay::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth() * 6 / 10, getHeight() * 3 / 10);
    auto area = panel.reduced(8);
    auto barHeight = 24;
    progressBar.setBounds(area.removeFromTop(barHeight));
    area.removeFromTop(4);
    auto btnHeight = 24;
    okButton.setBounds(area.removeFromBottom(btnHeight).removeFromRight(80));
    messageLabel.setBounds(area);
    ModalOverlay::resized();
}

bool ValidationOverlay::keyPressed(const juce::KeyPress& kp)
{
    if (state == ValidationOverlay::State::warning &&
        (kp == juce::KeyPress::returnKey || kp == juce::KeyPress::escapeKey))
    {
        okButton.triggerClick();
        return true;
    }
    // consume escape or other keys
    return true;
}

void ValidationOverlay::timerCallback()
{
    if (state == running)
    {
        progressBar.setProgress(progress.load());
    }
}

