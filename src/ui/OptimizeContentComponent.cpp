#include "OptimizeContentComponent.h"
#include "PluginLookAndFeel.h"
#include "../core/PluginProcessor.h"
#include "../utils/FormulaHelper.h"
#include "../utils/FormulaQuality.h"

OptimizeContentComponent::OptimizeContentComponent (NeuroCoreAudioProcessor& proc,
                                                    const juce::String& sourceScript)
    : processor (proc), original (sourceScript)
{
    titleLabel.setText ("Formula Optimizer", juce::dontSendNotification);
    titleLabel.setFont (NeuroCoreLookAndFeel::brandFont (18.f, true));
    titleLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    addAndMakeVisible (titleLabel);

    summaryLabel.setFont (NeuroCoreLookAndFeel::brandFont (13.f));
    summaryLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (summaryLabel);

    auto styleEd = [] (juce::TextEditor& e, bool ro)
    {
        e.setMultiLine (true, true);
        e.setReadOnly (ro);
        e.setFont (NeuroCoreLookAndFeel::monoFont (12.f));
        e.setColour (juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::surface());
        e.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe8ecf4));
        e.setColour (juce::TextEditor::outlineColourId, NeuroCoreLookAndFeel::panelBorder());
        e.setColour (juce::TextEditor::focusedOutlineColourId, NeuroCoreLookAndFeel::accent());
    };
    styleEd (beforeEditor, true);
    styleEd (afterEditor, true);
    styleEd (logEditor, true);
    beforeEditor.setText (original, false);
    addAndMakeVisible (beforeEditor);
    addAndMakeVisible (afterEditor);
    addAndMakeVisible (logEditor);

    beforeLabel.setText ("Original", juce::dontSendNotification);
    afterLabel.setText ("Optimized", juce::dontSendNotification);
    logLabel.setText ("Changes & quality", juce::dontSendNotification);
    for (auto* l : { &beforeLabel, &afterLabel, &logLabel })
    {
        l->setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
        l->setFont (NeuroCoreLookAndFeel::brandFont (11.f));
        addAndMakeVisible (*l);
    }

    applyButton.setColour (juce::TextButton::buttonColourId, NeuroCoreLookAndFeel::accentDim());
    applyButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    reRunButton.setColour (juce::TextButton::buttonColourId, NeuroCoreLookAndFeel::surfaceHigh());
    closeButton.setColour (juce::TextButton::buttonColourId, NeuroCoreLookAndFeel::surfaceHigh());
    addAndMakeVisible (applyButton);
    addAndMakeVisible (reRunButton);
    addAndMakeVisible (closeButton);

    applyButton.onClick = [this]
    {
        if (onApply)
            onApply (optimized.isNotEmpty() ? optimized : original);
    };
    reRunButton.onClick = [this] { runOptimize(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    runOptimize();
}

void OptimizeContentComponent::runOptimize()
{
    auto report = optimizeFormulaDetailed (original);
    optimized = report.script;

    juce::String log;
    if (report.changes == 0)
        log << "No safe rewrites found - script already optimal or protected.\n";
    else
        log << report.changes << " change(s) applied.\n";

    for (auto& m : report.messages)
        log << "• " << m << "\n";

    log << "\n- Quality check -\n";
    auto q = processor.analyseFormulaQuality (optimized.isNotEmpty() ? optimized : original);
    log << q.summary() << "\n";
    for (auto& e : q.errors)
        log << "ERR: " << e << "\n";
    for (auto& w : q.warnings)
        log << "WARN: " << w << "\n";

    afterEditor.setText (optimized, false);
    logEditor.setText (log, false);

    if (report.changes > 0)
    {
        summaryLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7dcea0));
        summaryLabel.setText (juce::String (report.changes) + " optimization(s) ready - review and Apply.",
                              juce::dontSendNotification);
        applyButton.setEnabled (true);
    }
    else
    {
        summaryLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
        summaryLabel.setText ("Nothing to change safely.", juce::dontSendNotification);
        applyButton.setEnabled (false);
    }
}

void OptimizeContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (NeuroCoreLookAndFeel::surface().withAlpha (0.4f));
}

void OptimizeContentComponent::resized()
{
    auto r = getLocalBounds().reduced (10);
    titleLabel.setBounds (r.removeFromTop (26));
    summaryLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);

    auto bottom = r.removeFromBottom (40);
    applyButton.setBounds (bottom.removeFromLeft (120).reduced (3));
    reRunButton.setBounds (bottom.removeFromLeft (100).reduced (3));
    closeButton.setBounds (bottom.removeFromRight (100).reduced (3));

    r.removeFromBottom (6);
    auto logArea = r.removeFromBottom (r.getHeight() / 4);
    logLabel.setBounds (logArea.removeFromTop (16));
    logEditor.setBounds (logArea);

    r.removeFromBottom (6);
    auto half = r.getWidth() / 2;
    auto left = r.removeFromLeft (half).reduced (0, 0);
    r.removeFromLeft (8);
    beforeLabel.setBounds (left.removeFromTop (16));
    beforeEditor.setBounds (left);
    afterLabel.setBounds (r.removeFromTop (16));
    afterEditor.setBounds (r);
}
