#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <unordered_map>

/**
    Manages MIDI CC → parameter mappings for MIDI Learn functionality.

    Thread-safety: mappings are protected by a SpinLock.
    The processMidiMessages() method is safe to call from the audio thread
    because it only reads the mapping under a TryLock and skips if the lock
    is contended, preventing audio-thread stalls.
*/
class MidiLearnManager
{
public:
    MidiLearnManager() = default;

    //==========================================================================
    // Mapping management (call from message thread)

    /** Map a MIDI CC number to a parameter ID. Replaces any existing mapping for that CC. */
    void setMapping(int ccNumber, const juce::String& paramId);

    /** Remove the mapping for a specific CC number. */
    void clearMapping(int ccNumber);

    /** Remove any CC mapping that points to the given parameter. */
    void clearMappingForParam(const juce::String& paramId);

    /** Returns the parameter ID mapped to the given CC, or an empty string. */
    juce::String getMappedParam(int ccNumber) const;

    /** Returns the CC number mapped to the given parameter, or -1 if none. */
    int getMappedCC(const juce::String& paramId) const;

    /** Returns true if there is any mapping registered. */
    bool hasMappings() const;

    //==========================================================================
    // MIDI Learn mode (call from message thread)

    /** Start listening for the next incoming CC message to map to paramId. */
    void startLearning(const juce::String& paramId);

    /** Stop MIDI Learn mode without assigning a mapping. */
    void stopLearning();

    /** Returns true while MIDI Learn mode is active. */
    bool isLearning() const noexcept { return learning.load(); }

    /** Returns the parameter ID currently waiting for a CC assignment. */
    juce::String getLearningParam() const;

    //==========================================================================
    // Audio thread: process incoming MIDI and apply parameter values

    /**
        Process all MIDI messages in the buffer.
        - If MIDI Learn is active, the first CC message assigns the mapping.
        - Otherwise, mapped CCs update their target parameters in the APVTS.
        Safe to call from the audio thread (uses a non-blocking TryLock).
    */
    void processMidiMessages(const juce::MidiBuffer& midiBuffer,
                             juce::AudioProcessorValueTreeState& apvts);

    //==========================================================================
    // State persistence

    /** Returns a ValueTree snapshot of the current mappings. */
    juce::ValueTree getState() const;

    /** Restore mappings from a previously saved ValueTree. */
    void setState(const juce::ValueTree& state);

private:
    mutable juce::SpinLock lock;

    std::unordered_map<int, juce::String> ccToParam;
    std::unordered_map<std::string, int>  paramToCC;

    std::atomic<bool> learning { false };
    juce::String      learningParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnManager)
};
