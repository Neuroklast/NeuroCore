#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>

inline void logError(const juce::String& msg)
{
    juce::Logger::writeToLog("[Error] " + msg);
}

inline void logWarning(const juce::String& msg)
{
    juce::Logger::writeToLog("[Warning] " + msg);
}
