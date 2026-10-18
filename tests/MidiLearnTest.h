#pragma once

#include <JuceHeader.h>
#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"
#include "TestHelpers.h"
#include <atomic>
#include <thread>
#if NK_ALLOCATION_PROBE
extern "C" void nkBeginAllocationProbe();
extern "C" unsigned nkEndAllocationProbe();
#endif

class MidiLearnTest : public juce::UnitTest
{
public:
    MidiLearnTest() : juce::UnitTest ("MidiLearn", "MIDI") {}

    void runTest() override
    {
        beginTest ("first learned CC does not allocate on the audio thread");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 32);
            proc.prepareToPlay (48000.0, 32);
            juce::AudioBuffer<float> buf (2, 32);
            juce::MidiBuffer empty;
            for (int i = 0; i < 4; ++i)
            {
                buf.clear();
                proc.processBlock (buf, empty);
            }

            proc.midiLearnManager.startLearning (EffectParameters::paramA);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 7, 64), 0);
           #if NK_ALLOCATION_PROBE
            nkBeginAllocationProbe();
           #endif
            proc.midiLearnManager.processMidiMessages (midi);
           #if NK_ALLOCATION_PROBE
            const auto allocations = nkEndAllocationProbe();
            expectEquals ((int) allocations, 0);
           #endif
            expect (! proc.midiLearnManager.isLearning());
            proc.midiLearnManager.drainLearnEvents();
            expectEquals (proc.midiLearnManager.getMappedCC (EffectParameters::paramA), 7);
            expectEquals (proc.midiLearnManager.getMappedParam (7), juce::String (EffectParameters::paramA));
        }

        beginTest ("learned CC drives the parameter on a 32-sample block");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 32);
            proc.prepareToPlay (48000.0, 32);
            proc.midiLearnManager.startLearning (EffectParameters::paramA);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 127), 0);
            juce::AudioBuffer<float> buf (2, 32);
            buf.clear();
            proc.processBlock (buf, midi);
            proc.midiLearnManager.drainLearnEvents();
            expectEquals (proc.midiLearnManager.getMappedCC (EffectParameters::paramA), 11);
            auto* p = proc.apvts.getParameter (EffectParameters::paramA);
            expect (p != nullptr);
            expectWithinAbsoluteError (p->getValue(), 1.0f, 0.001f);

            midi.clear();
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 0), 0);
            buf.clear();
            proc.processBlock (buf, midi);
            expectWithinAbsoluteError (p->getValue(), 0.0f, 0.001f);
        }

        beginTest ("mapping editor on another thread does not lock the audio path");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 64);
            proc.prepareToPlay (48000.0, 64);
            std::atomic<bool> stop { false };
            std::thread editor ([&]
            {
                int n = 0;
                while (! stop.load (std::memory_order_relaxed))
                {
                    proc.midiLearnManager.setMapping (n % 128, EffectParameters::paramA);
                    proc.midiLearnManager.clearMapping (n % 128);
                    ++n;
                }
            });

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 40), 0);
           #if NK_ALLOCATION_PROBE
            unsigned allocations = 0;
           #endif
            for (int i = 0; i < 64; ++i)
            {
                buf.clear();
               #if NK_ALLOCATION_PROBE
                nkBeginAllocationProbe();
               #endif
                proc.midiLearnManager.processMidiMessages (midi);
               #if NK_ALLOCATION_PROBE
                allocations += nkEndAllocationProbe();
               #endif
            }
            stop.store (true, std::memory_order_relaxed);
            editor.join();
           #if NK_ALLOCATION_PROBE
            expectEquals ((int) allocations, 0);
           #endif
            juce::AudioBuffer<float> out (2, 64);
            juce::MidiBuffer silent;
            out.clear();
            proc.processBlock (out, silent);
            expectEquals (TestHelpers::countNonFinite (out), 0);
        }
    }
};
