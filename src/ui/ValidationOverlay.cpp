#include "ValidationOverlay.h"
#include "../core/PluginProcessor.h"

ValidationOverlay::ValidationOverlay(NeuroCoreAudioProcessor& proc, const juce::String& expr)
    : processor(proc), script(expr)
{
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    setAlwaysOnTop(true);
    setWantsKeyboardFocus(true);

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
    if (worker && worker->joinable())
        worker->join();
}

void ValidationOverlay::startTest()
{
    worker = std::make_unique<std::thread>([this]() {
        juce::String warn;
        bool ok = processor.testFormulaStability(script, warn,
            [this](float p) { progress.store(p); });
        juce::MessageManager::callAsync([this, ok, warn]() {
            if (ok)
            {
                if (onResult)
                    onResult(true);
            }
            else
            {
                state = warning;
                warning = warn;
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

void ValidationOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.5f));
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(panel);
    g.setColour(juce::Colours::white);
    g.drawRect(panel);
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
}

bool ValidationOverlay::keyPressed(const juce::KeyPress& kp)
{
    if (state == warning && (kp == juce::KeyPress::returnKey || kp == juce::KeyPress::numberPadEnter))
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
        progressBar.repaint();
}

