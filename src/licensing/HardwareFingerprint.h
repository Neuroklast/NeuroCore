#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>

/// Utility class to generate a stable hardware fingerprint.
class HardwareFingerprint
{
public:
    /// Returns a SHA-256 hash derived from several system identifiers.
    static juce::String generate();
};
