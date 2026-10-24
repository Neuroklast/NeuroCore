#pragma once

#include <JuceHeader.h>

/** Opens the JUCE standalone device/sample-rate dialog when running as
    Standalone. No-op in VST3/AU/tests (no plugin holder). */
void tryOpenStandaloneAudioSettings();

/** Standalone only: replace JUCE's default title strip with the Web UI's
    integrated chrome. Safe to call repeatedly when the editor is reparented. */
void configureStandaloneWindowChrome (juce::Component& editor);

/** Window operations are restricted to the Standalone host and active editor.
    Returns false for missing window or unsupported action. Coordinates are
    bounded per call and represent JUCE logical desktop pixels. */
bool performStandaloneWindowAction (juce::Component& editor,
                                    const juce::String& action,
                                    int deltaX = 0,
                                    int deltaY = 0);
