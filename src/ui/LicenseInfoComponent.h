#pragma once

#include <JuceHeader.h>
#include "PluginLookAndFeel.h"

namespace LicenseInfoUi
{
inline juce::String holderLine (const juce::String& email)
{
    const auto e = email.trim();
    return e.isNotEmpty() ? e : juce::String ("Unknown holder");
}

inline juce::String issuedLine (const juce::String& issued)
{
    const auto d = issued.trim();
    return d.isNotEmpty() ? ("Issued  " + d) : juce::String();
}
} // namespace LicenseInfoUi

class LicenseInfoComponent : public juce::Component
{
public:
    LicenseInfoComponent (juce::String email, juce::String issued)
    {
        caption.setText ("Licensed to", juce::dontSendNotification);
        caption.setFont (NeuroKoreLookAndFeel::monoFont (13.f));
        caption.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::mutedText());
        caption.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (caption);

        holder.setText (LicenseInfoUi::holderLine (email), juce::dontSendNotification);
        holder.setFont (NeuroKoreLookAndFeel::brandFont (22.f, true));
        holder.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::accent());
        holder.setJustificationType (juce::Justification::centred);
        holder.setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (holder);

        const auto issuedText = LicenseInfoUi::issuedLine (issued);
        issuedLabel.setText (issuedText, juce::dontSendNotification);
        issuedLabel.setFont (NeuroKoreLookAndFeel::monoFont (14.f));
        issuedLabel.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::mutedText());
        issuedLabel.setJustificationType (juce::Justification::centred);
        issuedLabel.setVisible (issuedText.isNotEmpty());
        addAndMakeVisible (issuedLabel);

        note.setText ("This copy of NEUROKORE is activated.", juce::dontSendNotification);
        note.setFont (NeuroKoreLookAndFeel::monoFont (14.f));
        note.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::brightText());
        note.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (note);

        replaceButton.setButtonText ("Replace license");
        replaceButton.onClick = [this]
        {
            if (onReplace)
                onReplace();
        };
        addAndMakeVisible (replaceButton);

        closeButton.setButtonText ("Close");
        closeButton.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible (closeButton);
    }

    std::function<void()> onClose;
    std::function<void()> onReplace;

    void paint (juce::Graphics&) override {}

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        auto buttons = r.removeFromBottom (32);
        closeButton.setBounds (buttons.removeFromRight (90));
        buttons.removeFromRight (8);
        replaceButton.setBounds (buttons.removeFromRight (140));
        r.removeFromBottom (12);
        caption.setBounds (r.removeFromTop (22));
        holder.setBounds (r.removeFromTop (32));
        if (issuedLabel.isVisible())
            issuedLabel.setBounds (r.removeFromTop (22));
        r.removeFromTop (8);
        note.setBounds (r.removeFromTop (24));
    }

private:
    juce::Label caption;
    juce::Label holder;
    juce::Label issuedLabel;
    juce::Label note;
    juce::TextButton replaceButton;
    juce::TextButton closeButton;
};
