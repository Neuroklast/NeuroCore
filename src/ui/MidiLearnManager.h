#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <array>
#include <unordered_map>

/**
    MIDI CC → parameter mappings.

    Audio thread: 128-slot atomic parameter-pointer table, plus a one-slot
    learn event. No SpinLock, no juce::String, no map insert.
    Message thread: string maps, persistence, drainLearnEvents().
*/
class MidiLearnManager
{
public:
    static constexpr int kCcCount = 128;

    MidiLearnManager();

    void bind (juce::AudioProcessorValueTreeState& apvts) noexcept;

    //==========================================================================
    // Mapping management (message thread)

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
    // MIDI Learn mode (message thread)

    /** Start listening for the next incoming CC message to map to paramId. */
    void startLearning(const juce::String& paramId);

    /** Stop MIDI Learn mode without assigning a mapping. */
    void stopLearning();

    /** Apply a pending learn event from the audio thread. */
    void drainLearnEvents();

    /** Returns true while MIDI Learn mode is active. */
    bool isLearning() const noexcept { return learning.load (std::memory_order_acquire); }

    /** Returns the parameter ID currently waiting for a CC assignment. */
    juce::String getLearningParam() const;

    //==========================================================================
    // Audio thread

    /**
        Process CC messages.
        Learning: store one CC in the event slot and arm the table pointer.
        Playback: load the table pointer and write the parameter.
    */
    void processMidiMessages (const juce::MidiBuffer& midiBuffer) noexcept;

    //==========================================================================
    // State persistence (message thread)

    /** Returns a ValueTree snapshot of the current mappings. */
    juce::ValueTree getState();

    /** Restore mappings from a previously saved ValueTree. */
    void setState(const juce::ValueTree& state);

private:
    juce::AudioProcessorParameter* lookupParam (const juce::String& paramId) const noexcept;
    void storeTarget (int cc, juce::AudioProcessorParameter* param) noexcept;
    void publishMapping (int ccNumber, const juce::String& paramId);

    juce::AudioProcessorValueTreeState* apvts { nullptr };

    std::array<std::atomic<juce::AudioProcessorParameter*>, kCcCount> ccTarget {};
    std::atomic<juce::AudioProcessorParameter*> learningTarget { nullptr };
    std::atomic<int> pendingLearnCc { -1 };
    std::atomic<int> pendingLearnValue { 0 };
    std::atomic<bool> learning { false };

    mutable juce::SpinLock lock;
    std::unordered_map<int, juce::String> ccToParam;
    std::unordered_map<std::string, int>  paramToCC;
    juce::String learningParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnManager)
};
