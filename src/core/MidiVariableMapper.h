#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file MidiVariableMapper.h
    @brief Maps incoming MIDI messages to DSL variables accessible in stage formulas.

    Provides the following variables in all stage blocks:
    - midi_note   – last-note-priority MIDI note (0–127, 0 when none held)
    - midi_freq   – frequency of that note in Hz
    - midi_vel    – velocity of that note (0.0–1.0)
    - midi_gate   – 1.0 while any note is held, 0.0 otherwise
    - midi_bend   – pitch bend (-1.0 to +1.0)
    - midi_mod    – CC1 modulation wheel (0.0–1.0)

    Note-off of one key restores the previous held note. CC 120/123 (All Sound
    Off / All Notes Off) clears the held stack. Audio-thread state is atomic
    and a fixed 128-note stack — no heap, no juce::String.
*/

#include <JuceHeader.h>
#include <atomic>
#include <cstdint>
#include <unordered_map>

class MidiVariableMapper
{
public:
    MidiVariableMapper() = default;

    /** Apply every event in order (end-of-buffer state). */
    void processMidi (const juce::MidiBuffer& midiMessages);

    /** Apply one MIDI message. Audio thread. */
    void processEvent (const juce::MidiMessage& msg) noexcept;

    /** Walk the buffer in sample order: apply events at T, then fn(start, count)
        for the span until the next event or the block end. */
    template <typename Fn>
    void walkSpans (const juce::MidiBuffer& midi, int numSamples, Fn&& fn)
    {
        const int n = juce::jmax (0, numSamples);
        int cursor = 0;
        for (const auto meta : midi)
        {
            const int s = juce::jlimit (0, n, meta.samplePosition);
            if (s > cursor)
            {
                fn (cursor, s - cursor);
                cursor = s;
            }
            processEvent (meta.getMessage());
        }
        if (cursor < n)
            fn (cursor, n - cursor);
    }

    void reset() noexcept;

    float getVariable(const juce::String& name) const noexcept;
    void applyToVariables(std::unordered_map<juce::String, float>& variables) const;

    float getMidiNote() const noexcept  { return midiNote.load (std::memory_order_relaxed); }
    float getMidiFreq() const noexcept  { return midiFreq.load (std::memory_order_relaxed); }
    float getMidiVel()  const noexcept  { return midiVel.load (std::memory_order_relaxed); }
    float getMidiGate() const noexcept  { return midiGate.load (std::memory_order_relaxed); }
    float getMidiBend() const noexcept  { return midiBend.load (std::memory_order_relaxed); }
    float getMidiMod()  const noexcept  { return midiMod.load (std::memory_order_relaxed); }

private:
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void panic (bool zeroControllers) noexcept;
    void unhold (int note) noexcept;
    void publishTop() noexcept;

    static float noteToFreq (float note) noexcept;

    std::atomic<float> midiNote { 0.0f };
    std::atomic<float> midiFreq { 0.0f };
    std::atomic<float> midiVel  { 0.0f };
    std::atomic<float> midiGate { 0.0f };
    std::atomic<float> midiBend { 0.0f };
    std::atomic<float> midiMod  { 0.0f };

    struct Held
    {
        std::uint8_t note { 0 };
        std::uint8_t vel { 0 };
    };
    Held stack[128] {};
    bool held[128] {};
    int nHeld { 0 };
};
