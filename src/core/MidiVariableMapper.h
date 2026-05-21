#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file MidiVariableMapper.h
    @brief Maps incoming MIDI messages to DSL variables accessible in stage formulas.

    Provides the following variables in all stage blocks:
    - midi_note   – current MIDI note (0–127, 0 when no note active)
    - midi_freq   – frequency of the current note in Hz (440 * pow(2, (midi_note - 69) / 12))
    - midi_vel    – velocity (0.0–1.0)
    - midi_gate   – 1.0 when note active, 0.0 otherwise
    - midi_bend   – pitch bend (-1.0 to +1.0)
    - midi_mod    – CC1 modulation wheel (0.0–1.0)

    All state is held as std::atomic<float> making this class safe to read
    from the real-time audio thread without any locks.
*/

#include <JuceHeader.h>
#include <atomic>
#include <unordered_map>

class MidiVariableMapper
{
public:
    MidiVariableMapper() = default;

    /** Process all MIDI events in the buffer and update internal state.
        Safe to call from the audio thread. */
    void processMidi(const juce::MidiBuffer& midiMessages);

    /** Returns the current value for a given variable name.
        Returns 0.0f for unknown names. */
    float getVariable(const juce::String& name) const noexcept;

    /** Writes all MIDI variables into the provided variable map.
        Intended to be called just before SignalChain::processBlock. */
    void applyToVariables(std::unordered_map<juce::String, float>& variables) const;

    // Direct accessors (RT-safe)
    float getMidiNote() const noexcept  { return midiNote.load(); }
    float getMidiFreq() const noexcept  { return midiFreq.load(); }
    float getMidiVel()  const noexcept  { return midiVel.load(); }
    float getMidiGate() const noexcept  { return midiGate.load(); }
    float getMidiBend() const noexcept  { return midiBend.load(); }
    float getMidiMod()  const noexcept  { return midiMod.load(); }

private:
    std::atomic<float> midiNote { 0.0f };
    std::atomic<float> midiFreq { 0.0f };
    std::atomic<float> midiVel  { 0.0f };
    std::atomic<float> midiGate { 0.0f };
    std::atomic<float> midiBend { 0.0f };
    std::atomic<float> midiMod  { 0.0f };

    static float noteToFreq(float note) noexcept;
};
