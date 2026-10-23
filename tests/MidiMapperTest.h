#pragma once

#include <JuceHeader.h>
#include "../src/core/MidiVariableMapper.h"
#include "../src/dsl/SignalChain.h"
#include "TestHelpers.h"

class MidiMapperTest : public juce::UnitTest
{
public:
    MidiMapperTest() : juce::UnitTest ("MidiMapper", "MIDI") {}

    void runTest() override
    {
        beginTest ("event counter distinguishes repeated controller events from held values");
        {
            MidiVariableMapper mapper;
            expectEquals ((int) mapper.getEventCount(), 0);
            mapper.processEvent (juce::MidiMessage::controllerEvent (1, 1, 90));
            mapper.processEvent (juce::MidiMessage::controllerEvent (1, 1, 90));
            expectEquals ((int) mapper.getEventCount(), 2);
            juce::MidiBuffer empty;
            mapper.processMidi (empty);
            expectEquals ((int) mapper.getEventCount(), 2);
            mapper.processEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
            mapper.processEvent (juce::MidiMessage::noteOff (1, 60));
            mapper.processEvent (juce::MidiMessage::pitchWheel (1, 9000));
            expectEquals ((int) mapper.getEventCount(), 5);
        }

        beginTest ("last-note priority keeps gate when an earlier key is released");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80), 0);
            mapper.processMidi (midi);
            expectWithinAbsoluteError (mapper.getMidiNote(), 64.f, 0.5f);
            expectWithinAbsoluteError (mapper.getMidiGate(), 1.f, 0.01f);

            juce::MidiBuffer offEarly;
            offEarly.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            mapper.processMidi (offEarly);
            expectWithinAbsoluteError (mapper.getMidiGate(), 1.f, 0.01f);
            expectWithinAbsoluteError (mapper.getMidiNote(), 64.f, 0.5f);
        }

        beginTest ("releasing the current note falls back to the held key");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 0);
            mapper.processMidi (midi);
            juce::MidiBuffer offTop;
            offTop.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
            mapper.processMidi (offTop);
            expectWithinAbsoluteError (mapper.getMidiGate(), 1.f, 0.01f);
            expectWithinAbsoluteError (mapper.getMidiNote(), 60.f, 0.5f);
        }

        beginTest ("all notes off clears a held gate");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            mapper.processMidi (midi);
            juce::MidiBuffer panic;
            panic.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            mapper.processMidi (panic);
            expectWithinAbsoluteError (mapper.getMidiGate(), 0.f, 0.01f);
            expectWithinAbsoluteError (mapper.getMidiNote(), 0.f, 0.5f);
        }

        beginTest ("note on then off in one buffer keeps gate high until the off sample");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 32);
            float onPeak = 0.f, offPeak = 0.f;
            mapper.walkSpans (midi, 64, [&] (int start, int count)
            {
                const float g = mapper.getMidiGate();
                if (start < 32)
                    onPeak = juce::jmax (onPeak, g);
                if (start + count > 32)
                    offPeak = juce::jmax (offPeak, g);
            });
            expectWithinAbsoluteError (onPeak, 1.f, 0.01f);
            expectWithinAbsoluteError (offPeak, 0.f, 0.01f);
        }

        beginTest ("same-block note off is audible after the off sample");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("stage1: y = midi_gate", err), err);
            chain.prepare ({ 48000.0, 64, 1 });
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 32);
            juce::AudioBuffer<float> buf (1, 64);
            buf.clear();
            mapper.walkSpans (midi, 64, [&] (int start, int count)
            {
                chain.setMidiVariables (mapper);
                juce::AudioBuffer<float> slice (buf.getArrayOfWritePointers(), 1, start, count);
                chain.processBlock (slice);
            });
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expectWithinAbsoluteError (buf.getSample (0, 0), 1.f, 0.01f);
            expectWithinAbsoluteError (buf.getSample (0, 31), 1.f, 0.01f);
            expectWithinAbsoluteError (buf.getSample (0, 32), 0.f, 0.01f);
            expectWithinAbsoluteError (buf.getSample (0, 63), 0.f, 0.01f);
        }
    }
};
