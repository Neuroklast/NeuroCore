#pragma once

/** Opens the JUCE standalone device/sample-rate dialog when running as
    Standalone. No-op in VST3/AU/tests (no plugin holder). */
void tryOpenStandaloneAudioSettings();
