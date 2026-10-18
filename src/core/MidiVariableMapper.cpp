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
        processEvent (metadata.getMessage());
}

void MidiVariableMapper::processEvent (const juce::MidiMessage& msg) noexcept
{
    if (msg.isNoteOn())
        noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff())
        noteOff (msg.getNoteNumber());
    else if (msg.isAllSoundOff())
        panic (true);
    else if (msg.isAllNotesOff())
        panic (false);
    else if (msg.isPitchWheel())
    {
        const int raw = msg.getPitchWheelValue();
        const float bend = (raw - 8192) / 8192.0f;
        midiBend.store (juce::jlimit (-1.0f, 1.0f, bend), std::memory_order_relaxed);
    }
    else if (msg.isController() && msg.getControllerNumber() == 1)
        midiMod.store (static_cast<float> (msg.getControllerValue()) / 127.0f,
                       std::memory_order_relaxed);
}

void MidiVariableMapper::reset() noexcept
{
    panic (true);
}

void MidiVariableMapper::noteOn (int note, float velocity) noexcept
{
    if (note < 0 || note > 127)
        return;
    unhold (note);
    if (nHeld >= 128)
        return;
    const auto vel = (std::uint8_t) juce::jlimit (0, 127, (int) std::lround (velocity * 127.0f));
    stack[nHeld] = { (std::uint8_t) note, vel };
    ++nHeld;
    held[note] = true;
    publishTop();
}

void MidiVariableMapper::noteOff (int note) noexcept
{
    unhold (note);
    publishTop();
}

void MidiVariableMapper::panic (bool zeroControllers) noexcept
{
    nHeld = 0;
    for (bool& h : held)
        h = false;
    midiNote.store (0.f, std::memory_order_relaxed);
    midiFreq.store (0.f, std::memory_order_relaxed);
    midiVel.store (0.f, std::memory_order_relaxed);
    midiGate.store (0.f, std::memory_order_relaxed);
    if (zeroControllers)
    {
        midiBend.store (0.f, std::memory_order_relaxed);
        midiMod.store (0.f, std::memory_order_relaxed);
    }
}

void MidiVariableMapper::unhold (int note) noexcept
{
    if (note < 0 || note > 127 || ! held[note])
        return;
    held[note] = false;
    int w = 0;
    for (int i = 0; i < nHeld; ++i)
        if (stack[i].note != (std::uint8_t) note)
            stack[w++] = stack[i];
    nHeld = w;
}

void MidiVariableMapper::publishTop() noexcept
{
    if (nHeld <= 0)
    {
        midiNote.store (0.f, std::memory_order_relaxed);
        midiFreq.store (0.f, std::memory_order_relaxed);
        midiVel.store (0.f, std::memory_order_relaxed);
        midiGate.store (0.f, std::memory_order_relaxed);
        return;
    }
    const Held top = stack[nHeld - 1];
    midiNote.store ((float) top.note, std::memory_order_relaxed);
    midiFreq.store (noteToFreq ((float) top.note), std::memory_order_relaxed);
    midiVel.store ((float) top.vel / 127.0f, std::memory_order_relaxed);
    midiGate.store (1.0f, std::memory_order_relaxed);
}

float MidiVariableMapper::getVariable(const juce::String& name) const noexcept
{
    if (name == "midi_note")  return getMidiNote();
    if (name == "midi_freq")  return getMidiFreq();
    if (name == "midi_vel")   return getMidiVel();
    if (name == "midi_gate")  return getMidiGate();
    if (name == "midi_bend")  return getMidiBend();
    if (name == "midi_mod")   return getMidiMod();
    return 0.0f;
}

void MidiVariableMapper::applyToVariables(std::unordered_map<juce::String, float>& variables) const
{
    variables["midi_note"] = getMidiNote();
    variables["midi_freq"] = getMidiFreq();
    variables["midi_vel"]  = getMidiVel();
    variables["midi_gate"] = getMidiGate();
    variables["midi_bend"] = getMidiBend();
    variables["midi_mod"]  = getMidiMod();
}
