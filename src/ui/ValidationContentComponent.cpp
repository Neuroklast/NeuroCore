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

    startTimerHz(30);
    openedMs = juce::Time::getMillisecondCounter();
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

        // 1) Fast quality metric (static + dynamic probes)
        const auto quality = processor.analyseFormulaQuality (text);

        // 2) Existing long stability sweep
        bool stable = processor.testFormulaStability(text, warn, progressFn);
        if (abortRequested.load())
            return;

        const bool ok = stable && quality.ok;
        juce::String detail = warn;
        if (! quality.ok)
        {
            detail = quality.errors.joinIntoString ("; ");
            if (detail.isEmpty())
                detail = "Formula quality check failed";
        }
        else if (quality.warnings.size() > 0)
        {
            // Pass but surface warnings in stats
        }

        const auto qualityLine = quality.summary();
        const auto warnLine = quality.warnings.joinIntoString (" | ");

        juce::MessageManager::callAsync ([cb = onResult, ok, detail, qualityLine, warnLine, quality, this]() mutable {
            if (ok)
            {
                warningString = qualityLine;
                if (warnLine.isNotEmpty())
                    warningString << "  | " << warnLine;
                messageLabel.setText ("FORMULA STABLE", juce::dontSendNotification);
                statsLabel.setText (qualityLine
                                    + "  NaN: " + juce::String (quality.nanCount)
                                    + " Inf: " + juce::String (quality.infCount),
                                    juce::dontSendNotification);
                progressBar.setProgress (1.f);
                finishSuccess();
                juce::ignoreUnused (cb);
            }
            else
            {
                state = State::warning;
                warningString = detail;
                progressBar.setVisible (false);
                okButton.setVisible (true);
                icon.setVisible (true);
                messageLabel.setText (detail, juce::dontSendNotification);
                statsLabel.setText (qualityLine
                                    + "  NaN: " + juce::String (quality.nanCount)
                                    + " Inf: " + juce::String (quality.infCount),
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
        if (onProgress)
            onProgress ("NaN " + juce::String (nanCount.load())
                        + "  Inf " + juce::String (infCount.load())
                        + (txt.isNotEmpty() ? ("  |  " + txt) : juce::String()));
    }
}

void ValidationContentComponent::finishSuccess()
{
    const int elapsed = (int) (juce::Time::getMillisecondCounter() - openedMs);
    const int wait = juce::jmax (0, 1200 - elapsed);
    juce::Component::SafePointer<ValidationContentComponent> safe (this);
    juce::Timer::callAfterDelay (wait, [safe]()
    {
        if (safe == nullptr)
            return;
        if (safe->onResult)
            safe->onResult (true);
    });
}
