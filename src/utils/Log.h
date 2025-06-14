#pragma once
#include <JuceHeader.h>

inline void logError(const juce::String& msg)
{
    juce::Logger::writeToLog("[Error] " + msg);
}

inline void logWarning(const juce::String& msg)
{
    juce::Logger::writeToLog("[Warning] " + msg);
}
