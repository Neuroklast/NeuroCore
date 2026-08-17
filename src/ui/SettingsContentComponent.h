#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"
#include "../utils/UiSettings.h"
#include "PluginLookAndFeel.h"
#include "StandaloneAudioSettings.h"
#include "fx/CyberFxTypes.h"

namespace SettingsUi
{
inline juce::String motionCaption (CyberMotion m)
{
    switch (m)
    {
        case CyberMotion::Reduced: return "Reduced";
        case CyberMotion::Off:     return "Off";
        case CyberMotion::Full:
        default:                   return "Full";
    }
}

inline juce::String motionHint (CyberMotion m)
{
    switch (m)
    {
        case CyberMotion::Reduced: return "SNAP — overlays open instantly. No boot. No glitch.";
        case CyberMotion::Off:     return "Still chrome. No motion at all.";
        case CyberMotion::Full:
        default:                   return "Boot, glitch, CRT glass, overlay cinema.";
    }
}
} // namespace SettingsUi

/** Prefs panel: animation, UI scale, formula text, standalone audio. */
class SettingsContentComponent : public juce::Component
{
public:
    SettingsContentComponent()
    {
        auto styleLabel = [] (juce::Label& l, float pt, juce::Colour c, juce::Justification j)
        {
            l.setFont (NeuroKoreLookAndFeel::monoFont (pt));
            l.setColour (juce::Label::textColourId, c);
            l.setJustificationType (j);
        };

        animTitle.setText ("ANIMATION", juce::dontSendNotification);
        styleLabel (animTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (animTitle);

        animHint.setText (SettingsUi::motionHint (UiSettings::get().motion()),
                          juce::dontSendNotification);
        styleLabel (animHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        animHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (animHint);

        const CyberMotion choices[] = { CyberMotion::Full, CyberMotion::Reduced, CyberMotion::Off };
        for (int i = 0; i < 3; ++i)
        {
            auto& b = motionButtons[i];
            b.setButtonText (SettingsUi::motionCaption (choices[i]));
            b.setClickingTogglesState (true);
            b.setRadioGroupId (0x4d4f544e);
            b.onClick = [this, m = choices[i]]
            {
                UiSettings::get().setMotion (m);
                animHint.setText (SettingsUi::motionHint (m), juce::dontSendNotification);
                if (onMotionChanged)
                    onMotionChanged (m);
            };
            addAndMakeVisible (b);
        }

        procTitle.setText ("PROCESSING", juce::dontSendNotification);
        styleLabel (procTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (procTitle);
        procHint.setText (UiSettings::get().liveMode()
                              ? "Live: min-phase OS, near-zero latency."
                              : "Studio: linear-phase OS, mix-ready.",
                          juce::dontSendNotification);
        styleLabel (procHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        procHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (procHint);

        studioButton.setButtonText ("Studio");
        liveButton.setButtonText ("Live");
        studioButton.setClickingTogglesState (true);
        liveButton.setClickingTogglesState (true);
        studioButton.setRadioGroupId (0x4c495645);
        liveButton.setRadioGroupId (0x4c495645);
        studioButton.setTooltip ("Linear-phase oversampling. Accurate for mixing.");
        liveButton.setTooltip ("Min-phase oversampling. Playing live without the delay.");
        studioButton.onClick = [this]
        {
            if (onLiveModeChanged)
                onLiveModeChanged (false);
            procHint.setText ("Studio: linear-phase OS, mix-ready.", juce::dontSendNotification);
        };
        liveButton.onClick = [this]
        {
            if (onLiveModeChanged)
                onLiveModeChanged (true);
            procHint.setText ("Live: min-phase OS, near-zero latency.", juce::dontSendNotification);
        };
        addAndMakeVisible (studioButton);
        addAndMakeVisible (liveButton);

        displayTitle.setText ("DISPLAY", juce::dontSendNotification);
        styleLabel (displayTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (displayTitle);

        scaleHint.setText ("Window size. Aspect stays locked.", juce::dontSendNotification);
        styleLabel (scaleHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        scaleHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (scaleHint);

        const int percents[] = { 100, 125, 150 };
        for (int i = 0; i < 3; ++i)
        {
            auto& b = scaleButtons[i];
            b.setButtonText (juce::String (percents[i]) + "%");
            b.setClickingTogglesState (true);
            b.setRadioGroupId (0x53434c45);
            b.onClick = [this, p = percents[i]]
            {
                UiSettings::get().setUiScalePercent (p);
                if (onScaleChanged)
                    onScaleChanged (p);
            };
            addAndMakeVisible (b);
        }

        fontHint.setText ("Formula text size", juce::dontSendNotification);
        styleLabel (fontHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        fontHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (fontHint);
        fontSize.setJustificationType (juce::Justification::centred);
        styleLabel (fontSize, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centred);
        addAndMakeVisible (fontSize);
        fontMinus.setButtonText ("Aa-");
        fontPlus.setButtonText ("Aa+");
        fontMinus.setTooltip ("Smaller formula text");
        fontPlus.setTooltip ("Larger formula text");
        fontMinus.onClick = [this]
        {
            if (onFontStep)
                onFontStep (-(int) Config::kEditorFontStepPt);
        };
        fontPlus.onClick = [this]
        {
            if (onFontStep)
                onFontStep ((int) Config::kEditorFontStepPt);
        };
        addAndMakeVisible (fontMinus);
        addAndMakeVisible (fontPlus);

        cableTitle.setText ("CIRCUIT CABLES", juce::dontSendNotification);
        styleLabel (cableTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (cableTitle);
        cableHint.setText (UiSettings::get().cableWaveform()
                               ? "Gray waveform on the cable, only while audio is present."
                               : "White/gray traces; beads travel only while audio is present.",
                           juce::dontSendNotification);
        styleLabel (cableHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        cableHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (cableHint);
        dotsCableButton.setButtonText ("Dots");
        waveCableButton.setButtonText ("Wave");
        dotsCableButton.setClickingTogglesState (true);
        waveCableButton.setClickingTogglesState (true);
        dotsCableButton.setRadioGroupId (0x43424c45);
        waveCableButton.setRadioGroupId (0x43424c45);
        dotsCableButton.setTooltip ("White/gray traces with beads that follow the input level.");
        waveCableButton.setTooltip ("Gray post-block waveform on the same traces.");
        dotsCableButton.onClick = [this]
        {
            UiSettings::get().setCableWaveform (false);
            cableHint.setText ("White/gray traces; beads travel only while audio is present.",
                               juce::dontSendNotification);
            if (onCableStyleChanged)
                onCableStyleChanged();
        };
        waveCableButton.onClick = [this]
        {
            UiSettings::get().setCableWaveform (true);
            cableHint.setText ("Gray waveform on the cable, only while audio is present.",
                               juce::dontSendNotification);
            if (onCableStyleChanged)
                onCableStyleChanged();
        };
        addAndMakeVisible (dotsCableButton);
        addAndMakeVisible (waveCableButton);

        tempoTitle.setText ("TEMPO", juce::dontSendNotification);
        styleLabel (tempoTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (tempoTitle);
        tempoHint.setText (UiSettings::get().useHostTempo()
                               ? "BPM follows the host / DAW."
                               : "BPM is set here. Delay note lengths use this tempo.",
                           juce::dontSendNotification);
        styleLabel (tempoHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        tempoHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (tempoHint);
        hostTempoButton.setButtonText ("Host");
        userTempoButton.setButtonText ("User");
        hostTempoButton.setClickingTogglesState (true);
        userTempoButton.setClickingTogglesState (true);
        hostTempoButton.setRadioGroupId (0x54454d50);
        userTempoButton.setRadioGroupId (0x54454d50);
        hostTempoButton.setTooltip ("Use the DAW / host tempo.");
        userTempoButton.setTooltip ("Ignore the host. Type a BPM.");
        hostTempoButton.onClick = [this]
        {
            UiSettings::get().setUseHostTempo (true);
            bpmEdit.setEnabled (false);
            tempoHint.setText ("BPM follows the host / DAW.", juce::dontSendNotification);
        };
        userTempoButton.onClick = [this]
        {
            UiSettings::get().setUseHostTempo (false);
            bpmEdit.setEnabled (true);
            tempoHint.setText ("BPM is set here. Delay note lengths use this tempo.",
                               juce::dontSendNotification);
        };
        addAndMakeVisible (hostTempoButton);
        addAndMakeVisible (userTempoButton);
        bpmLabel.setText ("BPM", juce::dontSendNotification);
        styleLabel (bpmLabel, 13.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (bpmLabel);
        bpmEdit.setFont (NeuroKoreLookAndFeel::monoFont (16.f));
        bpmEdit.setInputRestrictions (6, "0123456789.");
        bpmEdit.setText (juce::String (UiSettings::get().userBpm(), 1), false);
        bpmEdit.setEnabled (! UiSettings::get().useHostTempo());
        bpmEdit.onReturnKey = [this] { commitUserBpm(); };
        bpmEdit.onFocusLost = [this] { commitUserBpm(); };
        addAndMakeVisible (bpmEdit);

        audioTitle.setText ("AUDIO", juce::dontSendNotification);
        styleLabel (audioTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (audioTitle);

        const bool standalone = juce::JUCEApplicationBase::isStandaloneApp();
        audioHint.setText (standalone
                               ? "Device, sample rate, and buffer. Host plugins follow the DAW."
                               : "This copy is hosted. Sample rate and device belong to the DAW.",
                           juce::dontSendNotification);
        styleLabel (audioHint, 16.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        audioHint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (audioHint);

        audioButton.setButtonText ("Audio device...");
        audioButton.setTooltip ("Sample rate / device (Standalone).");
        audioButton.onClick = [] { tryOpenStandaloneAudioSettings(); };
        audioButton.setEnabled (standalone);
        audioButton.setVisible (standalone);
        addAndMakeVisible (audioButton);

        aboutTitle.setText ("ABOUT", juce::dontSendNotification);
        styleLabel (aboutTitle, 14.f, NeuroKoreLookAndFeel::ink(), juce::Justification::centredLeft);
        addAndMakeVisible (aboutTitle);
        licenseButton.setButtonText ("License");
        helpButton.setButtonText ("Help / Manual");
        licenseButton.onClick = [this] { if (onLicense) onLicense(); };
        helpButton.onClick = [this] { if (onHelp) onHelp(); };
        addAndMakeVisible (licenseButton);
        addAndMakeVisible (helpButton);

        closeButton.setButtonText ("Close");
        closeButton.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible (closeButton);

        syncFromSettings();
    }

    std::function<void (CyberMotion)> onMotionChanged;
    std::function<void (int)>         onScaleChanged;
    std::function<void (int)>         onFontStep;
    std::function<void (bool)>        onLiveModeChanged;
    std::function<void()>             onCableStyleChanged;
    std::function<void()>             onLicense;
    std::function<void()>             onHelp;
    std::function<void()>             onClose;

    void setFontPt (float pt)
    {
        fontSize.setText (juce::String ((int) std::round (pt)) + " pt",
                          juce::dontSendNotification);
    }

    void paint (juce::Graphics&) override {}

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        auto buttons = r.removeFromBottom (32);
        closeButton.setBounds (buttons.removeFromRight (90));
        r.removeFromBottom (10);

        auto row = [&r] (int h) { return r.removeFromTop (h); };
        animTitle.setBounds (row (20));
        {
            auto bar = row (32);
            const int w = bar.getWidth() / 3;
            motionButtons[0].setBounds (bar.removeFromLeft (w).reduced (2));
            motionButtons[1].setBounds (bar.removeFromLeft (w).reduced (2));
            motionButtons[2].setBounds (bar.reduced (2));
        }
        animHint.setBounds (row (36));
        r.removeFromTop (10);

        procTitle.setBounds (row (18));
        {
            auto bar = row (30);
            const int w = bar.getWidth() / 2;
            studioButton.setBounds (bar.removeFromLeft (w).reduced (2));
            liveButton.setBounds (bar.reduced (2));
        }
        procHint.setBounds (row (22));
        r.removeFromTop (10);

        displayTitle.setBounds (row (18));
        {
            auto bar = row (30);
            const int w = bar.getWidth() / 3;
            scaleButtons[0].setBounds (bar.removeFromLeft (w).reduced (2));
            scaleButtons[1].setBounds (bar.removeFromLeft (w).reduced (2));
            scaleButtons[2].setBounds (bar.reduced (2));
        }
        scaleHint.setBounds (row (20));
        r.removeFromTop (6);
        {
            auto bar = row (30);
            fontHint.setBounds (bar.removeFromLeft (160));
            fontMinus.setBounds (bar.removeFromLeft (56).reduced (2));
            fontSize.setBounds (bar.removeFromLeft (72));
            fontPlus.setBounds (bar.removeFromLeft (56).reduced (2));
        }
        r.removeFromTop (12);

        cableTitle.setBounds (row (18));
        {
            auto bar = row (30);
            const int w = bar.getWidth() / 2;
            dotsCableButton.setBounds (bar.removeFromLeft (w).reduced (2));
            waveCableButton.setBounds (bar.reduced (2));
        }
        cableHint.setBounds (row (22));
        r.removeFromTop (12);

        tempoTitle.setBounds (row (18));
        {
            auto bar = row (30);
            const int w = bar.getWidth() / 2;
            hostTempoButton.setBounds (bar.removeFromLeft (w).reduced (2));
            userTempoButton.setBounds (bar.reduced (2));
        }
        tempoHint.setBounds (row (22));
        {
            auto bar = row (30);
            bpmLabel.setBounds (bar.removeFromLeft (48));
            bpmEdit.setBounds (bar.removeFromLeft (80).reduced (0, 2));
        }
        r.removeFromTop (12);

        audioTitle.setBounds (row (18));
        audioHint.setBounds (row (36));
        if (audioButton.isVisible())
            audioButton.setBounds (row (30).removeFromLeft (180));
        r.removeFromTop (12);
        aboutTitle.setBounds (row (18));
        {
            auto bar = row (30);
            licenseButton.setBounds (bar.removeFromLeft (bar.getWidth() / 2).reduced (2));
            helpButton.setBounds (bar.reduced (2));
        }
    }

private:
    void syncFromSettings()
    {
        const auto m = UiSettings::get().motion();
        motionButtons[0].setToggleState (m == CyberMotion::Full,     juce::dontSendNotification);
        motionButtons[1].setToggleState (m == CyberMotion::Reduced,  juce::dontSendNotification);
        motionButtons[2].setToggleState (m == CyberMotion::Off,      juce::dontSendNotification);
        animHint.setText (SettingsUi::motionHint (m), juce::dontSendNotification);

        const int p = UiSettings::get().uiScalePercent();
        scaleButtons[0].setToggleState (p == 100, juce::dontSendNotification);
        scaleButtons[1].setToggleState (p == 125, juce::dontSendNotification);
        scaleButtons[2].setToggleState (p == 150, juce::dontSendNotification);

        setFontPt (UiSettings::get().editorFontPt());

        const bool live = UiSettings::get().liveMode();
        studioButton.setToggleState (! live, juce::dontSendNotification);
        liveButton.setToggleState (live, juce::dontSendNotification);
        procHint.setText (live ? "Live: min-phase OS, near-zero latency."
                               : "Studio: linear-phase OS, mix-ready.",
                          juce::dontSendNotification);

        const bool hostT = UiSettings::get().useHostTempo();
        hostTempoButton.setToggleState (hostT, juce::dontSendNotification);
        userTempoButton.setToggleState (! hostT, juce::dontSendNotification);
        bpmEdit.setEnabled (! hostT);
        bpmEdit.setText (juce::String (UiSettings::get().userBpm(), 1), false);
        tempoHint.setText (hostT ? "BPM follows the host / DAW."
                                 : "BPM is set here. Delay note lengths use this tempo.",
                           juce::dontSendNotification);

        const bool wave = UiSettings::get().cableWaveform();
        dotsCableButton.setToggleState (! wave, juce::dontSendNotification);
        waveCableButton.setToggleState (wave, juce::dontSendNotification);
        cableHint.setText (wave ? "Cables draw the post-block waveform."
                                : "Cables show traveling dots, like a live trace.",
                           juce::dontSendNotification);
    }

    void commitUserBpm()
    {
        UiSettings::get().setUserBpm (bpmEdit.getText().getFloatValue());
        bpmEdit.setText (juce::String (UiSettings::get().userBpm(), 1), false);
    }

    juce::Label animTitle, animHint, procTitle, procHint, displayTitle, scaleHint, fontHint, fontSize,
                cableTitle, cableHint, tempoTitle, tempoHint, bpmLabel, audioTitle, audioHint, aboutTitle;
    juce::TextButton motionButtons[3];
    juce::TextButton scaleButtons[3];
    juce::TextButton studioButton, liveButton;
    juce::TextButton hostTempoButton, userTempoButton;
    juce::TextButton dotsCableButton, waveCableButton;
    juce::TextEditor bpmEdit;
    juce::TextButton fontMinus, fontPlus, audioButton, licenseButton, helpButton, closeButton;
};
