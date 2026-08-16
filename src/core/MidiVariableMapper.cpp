/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "MidiVariableMapper.h"
#include <cmath>

float MidiVariableMapper::noteToFreq(float note) noexcept
{
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

void MidiVariableMapper::processMidi(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const float note = static_cast<float>(msg.getNoteNumber());
            midiNote.store(note);
            midiFreq.store(noteToFreq(note));
            midiVel.store(msg.getFloatVelocity());
            midiGate.store(1.0f);
        }
        else if (msg.isNoteOff())
        {
            midiGate.store(0.0f);
            midiVel.store(0.0f);
        }
        else if (msg.isPitchWheel())
        {
            // Pitch wheel value in range [-1, 1]
            const int raw = msg.getPitchWheelValue(); // 0..16383, centre 8192
            const float bend = (raw - 8192) / 8192.0f;
            midiBend.store(juce::jlimit(-1.0f, 1.0f, bend));
        }
        else if (msg.isController())
        {
            if (msg.getControllerNumber() == 1) // CC1 = modulation wheel
            {
                midiMod.store(static_cast<float>(msg.getControllerValue()) / 127.0f);
            }
        }
    }
}

float MidiVariableMapper::getVariable(const juce::String& name) const noexcept
{
    if (name == "midi_note")  return midiNote.load();
    if (name == "midi_freq")  return midiFreq.load();
    if (name == "midi_vel")   return midiVel.load();
    if (name == "midi_gate")  return midiGate.load();
    if (name == "midi_bend")  return midiBend.load();
    if (name == "midi_mod")   return midiMod.load();
    return 0.0f;
}

void MidiVariableMapper::applyToVariables(std::unordered_map<juce::String, float>& variables) const
{
    variables["midi_note"] = midiNote.load();
    variables["midi_freq"] = midiFreq.load();
    variables["midi_vel"]  = midiVel.load();
    variables["midi_gate"] = midiGate.load();
    variables["midi_bend"] = midiBend.load();
    variables["midi_mod"]  = midiMod.load();
}
