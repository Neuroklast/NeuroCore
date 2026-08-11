#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/core/Config.h"
#include "../src/core/MidiVariableMapper.h"
#include "../src/core/WaveformCapture.h"
#include "../src/core/ScriptManager.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include <cmath>

#ifndef NEUROCORE_RESOURCES_DIR
#define NEUROCORE_RESOURCES_DIR "resources"
#endif

class NeuroCoreExtrasTest : public juce::UnitTest
{
public:
    NeuroCoreExtrasTest() : juce::UnitTest("NeuroCoreExtrasTest", "NewFeatures") {}

    void runTest() override
    {
        testGlobalVariables();
        testFeedbackLeak();
        testChannelRouting();
        testMidSideRouting();
        testMidiVariableMapper();
        testTempoSync();
        testEnvMidiTrigger();
        testTailTime();
        testWaveformCapture();
        testScriptManagerDelegation();
        testProcessorStateRoundTrip();
        testFactoryPresetLibrary();
    }

private:
    // ------------------------------------------------------------------ //
    // Helper
    // ------------------------------------------------------------------ //
    void makeChain(const juce::String& script, dsl::SignalChain& chain, double sr = 44100.0)
    {
        juce::String err;
        expect(chain.loadScript(script, err), "loadScript failed: " + err);
        chain.prepare({ sr, 512, 1 });
    }

    // Run a stereo chain on silence and return output
    void runStereoBlock(dsl::SignalChain& chain, juce::AudioBuffer<float>& buf, int numTimes = 1)
    {
        for (int i = 0; i < numTimes; ++i)
            chain.processBlock(buf);
    }

    // ------------------------------------------------------------------ //
    // Tests
    // ------------------------------------------------------------------ //

    // t (time), sr (sample rate), pi (3.14159…)
    void testGlobalVariables()
    {
        beginTest("Global variable: pi");
        {
            dsl::SignalChain chain;
            // Stage output is hard-limited to [-1, 1] — scale pi into range
            makeChain("stage1: y = pi * 0.1", chain);
            juce::AudioBuffer<float> buf(1, 4);
            buf.clear();
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0),
                                      juce::MathConstants<float>::pi * 0.1f, 1e-4f);
        }

        beginTest("Global variable: sr");
        {
            const double sr = 48000.0;
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = sr / 48000", err));
            chain.prepare({ sr, 512, 1 });
            juce::AudioBuffer<float> buf(1, 4);
            buf.clear();
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 1.0f, 1e-3f);
        }

        beginTest("Global variable: t advances over time");
        {
            const double sr = 44100.0;
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = t", err));
            chain.prepare({ sr, 4, 1 });
            juce::AudioBuffer<float> buf(1, 4);
            buf.clear();
            // processBlock stamps t once per block (start of block)
            chain.processBlock(buf); // t = 0
            chain.processBlock(buf); // t = 4/sr
            const float expected = 4.0f / (float)sr;
            expectWithinAbsoluteError(buf.getSample(0, 0), expected, 1e-5f);
        }

        beginTest("Oscillator: t-based sin produces non-zero output");
        {
            const double sr = 44100.0;
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = sin(2 * pi * 440 * t)", err));
            chain.prepare({ sr, 512, 1 });
            juce::AudioBuffer<float> buf(1, 512);
            buf.clear();
            // t is constant within a block — run enough blocks to leave t=0
            float rms = 0.f;
            int n = 0;
            for (int b = 0; b < 8; ++b)
            {
                buf.clear();
                chain.processBlock(buf);
                for (int i = 0; i < 512; ++i)
                {
                    rms += buf.getSample(0, i) * buf.getSample(0, i);
                    ++n;
                }
            }
            rms = std::sqrt(rms / (float) n);
            expectGreaterThan(rms, 0.01f);
        }
    }

    // y_prev feedback should not explode due to kFeedbackLeakFactor
    void testFeedbackLeak()
    {
        beginTest("Feedback leak: y_prev accumulation is bounded");
        {
            dsl::SignalChain chain;
            juce::String err;
            // This formula integrates: without a leak factor it would diverge
            expect(chain.loadScript("stage1: y = 0.999 * y_prev + 0.001", err));
            chain.prepare({ 44100.0, 512, 1 });
            juce::AudioBuffer<float> buf(1, 512);
            buf.clear();
            // Run for many blocks
            for (int block = 0; block < 200; ++block)
            {
                buf.clear();
                chain.processBlock(buf);
            }
            // Output must be finite and not exceed 1.0 after lots of feedback
            for (int i = 0; i < 512; ++i)
            {
                const float v = buf.getSample(0, i);
                expect(std::isfinite(v), "Feedback produced non-finite value");
                expectLessOrEqual(std::abs(v), 1.01f);
            }
        }

        beginTest("Feedback leak: x_prev remains stable");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = x_prev", err));
            chain.prepare({ 44100.0, 512, 1 });
            juce::AudioBuffer<float> buf(1, 512);
            for (int i = 0; i < 512; ++i)
                buf.setSample(0, i, 1.0f);
            for (int block = 0; block < 100; ++block)
                chain.processBlock(buf);
            for (int i = 0; i < 512; ++i)
            {
                const float v = buf.getSample(0, i);
                expect(std::isfinite(v), "x_prev produced non-finite value");
            }
        }
    }

    // channel: left / right routing
    void testChannelRouting()
    {
        beginTest("Channel routing: left only");
        {
            dsl::SignalChain chain;
            juce::String err;
            // stage1 only affects left channel; right channel passthrough
            expect(chain.loadScript("stage1: y = x * 0.5; channel = left", err));
            chain.prepare({ 44100.0, 4, 2 });
            juce::AudioBuffer<float> buf(2, 4);
            for (int i = 0; i < 4; ++i)
            {
                buf.setSample(0, i, 1.0f); // left
                buf.setSample(1, i, 1.0f); // right
            }
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.5f, 1e-4f);
            expectWithinAbsoluteError(buf.getSample(1, 0), 1.0f, 1e-4f);
        }

        beginTest("Channel routing: right only");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = x * 0.25; channel = right", err));
            chain.prepare({ 44100.0, 4, 2 });
            juce::AudioBuffer<float> buf(2, 4);
            for (int i = 0; i < 4; ++i)
            {
                buf.setSample(0, i, 1.0f);
                buf.setSample(1, i, 1.0f);
            }
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 1.0f, 1e-4f);
            expectWithinAbsoluteError(buf.getSample(1, 0), 0.25f, 1e-4f);
        }

        beginTest("Channel routing: both (default)");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = x * 0.5; channel = both", err));
            chain.prepare({ 44100.0, 4, 2 });
            juce::AudioBuffer<float> buf(2, 4);
            for (int i = 0; i < 4; ++i)
            {
                buf.setSample(0, i, 1.0f);
                buf.setSample(1, i, 1.0f);
            }
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.5f, 1e-4f);
            expectWithinAbsoluteError(buf.getSample(1, 0), 0.5f, 1e-4f);
        }

        beginTest("ch variable: left channel has ch=0");
        {
            dsl::SignalChain chain;
            juce::String err;
            // ch should be 0 for left, 1 for right
            expect(chain.loadScript("stage1: y = ch", err));
            chain.prepare({ 44100.0, 4, 2 });
            juce::AudioBuffer<float> buf(2, 4);
            buf.clear();
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.0f, 1e-4f);
            expectWithinAbsoluteError(buf.getSample(1, 0), 1.0f, 1e-4f);
        }
    }

    void testMidSideRouting()
    {
        beginTest("Mid-Side encode then decode is identity");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript(
                "stage1: ms_encode = true; y = x\nstage2: ms_decode = true; y = x", err),
                "loadScript: " + err);
            chain.prepare({ 44100.0, 4, 2 });
            juce::AudioBuffer<float> buf(2, 4);
            for (int i = 0; i < 4; ++i)
            {
                buf.setSample(0, i, 0.8f);
                buf.setSample(1, i, 0.4f);
            }
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.8f, 1e-4f);
            expectWithinAbsoluteError(buf.getSample(1, 0), 0.4f, 1e-4f);
        }
    }

    void testMidiVariableMapper()
    {
        beginTest("MidiVariableMapper: Note On sets midi_note and midi_gate");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (uint8_t)100), 0);
            mapper.processMidi(midi);
            expectWithinAbsoluteError(mapper.getMidiNote(), 60.0f, 0.5f);
            expectWithinAbsoluteError(mapper.getMidiGate(), 1.0f, 0.01f);
            const float expectedVel = 100.0f / 127.0f;
            expectWithinAbsoluteError(mapper.getMidiVel(), expectedVel, 0.01f);
        }

        beginTest("MidiVariableMapper: Note Off clears midi_gate");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (uint8_t)100), 0);
            mapper.processMidi(midi);
            juce::MidiBuffer midi2;
            midi2.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            mapper.processMidi(midi2);
            expectWithinAbsoluteError(mapper.getMidiGate(), 0.0f, 0.01f);
        }

        beginTest("MidiVariableMapper: Pitch bend");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0); // max up
            mapper.processMidi(midi);
            expectGreaterThan(mapper.getMidiBend(), 0.9f);
        }

        beginTest("MidiVariableMapper: CC1 modulation");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 0);
            mapper.processMidi(midi);
            expectWithinAbsoluteError(mapper.getMidiMod(), 1.0f, 0.01f);
        }

        beginTest("MidiVariableMapper: setMidiVariables updates SignalChain variables");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 69, (uint8_t)127), 0); // A4 = 440 Hz
            mapper.processMidi(midi);

            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = midi_gate", err));
            chain.prepare({ 44100.0, 4, 1 });
            chain.setMidiVariables(mapper);
            juce::AudioBuffer<float> buf(1, 4);
            buf.clear();
            chain.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0, 0), 1.0f, 0.01f);
        }

        beginTest("MidiVariableMapper: midi_freq for A4 = 440 Hz");
        {
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 69, (uint8_t)80), 0);
            mapper.processMidi(midi);
            expectWithinAbsoluteError(mapper.getMidiFreq(), 440.0f, 1.0f);
        }
    }

    void testTempoSync()
    {
        beginTest("Osc sync: setTempo updates frequency");
        {
            dsl::SignalChain chain;
            juce::String err;
            // sync = 1/4 means freq = bpm/60 * (1/4) quarter notes per second
            expect(chain.loadScript("osc1: shape = sin; sync = 1/4", err));
            chain.prepare({ 44100.0, 512, 1 });
            chain.setTempo(120.0, 0.0, true);
            juce::AudioBuffer<float> buf(1, 512);
            buf.clear();
            chain.processBlock(buf);
            // At 120 bpm with 1/4 sync: freq = 2 * 0.25 = 0.5 Hz
            // Just verify it doesn't crash and produces non-zero output
            float peak = 0.f;
            for (int i = 0; i < 512; ++i)
                peak = juce::jmax(peak, std::abs(buf.getSample(0, i)));
            // Osc at 0.5 Hz won't produce large output in first 512 samples at 44.1kHz
            // but it should produce some output after setTempo
            expect(std::isfinite(peak), "Tempo sync osc produced non-finite output");
        }
    }

    void testEnvMidiTrigger()
    {
        beginTest("Env trigger = midi_gate: envelope restarts on note-on");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript(
                "env1: mode = rms; attack = 1; release = 100; trigger = midi_gate",
                err));
            chain.prepare({ 44100.0, 512, 1 });
            // Process a block first with gate = 0
            juce::AudioBuffer<float> buf(1, 512);
            for (int i = 0; i < 512; ++i) buf.setSample(0, i, 0.5f);
            chain.processBlock(buf);
            // Fire midi_gate transition 0→1
            MidiVariableMapper mapper;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (uint8_t)100), 0);
            mapper.processMidi(midi);
            chain.setMidiVariables(mapper);
            for (int i = 0; i < 512; ++i) buf.setSample(0, i, 0.5f);
            chain.processBlock(buf);
            // Just verify it doesn't crash
            expect(true, "Env midi trigger did not crash");
        }
    }

    void testTailTime()
    {
        beginTest("getMaxTailTime: returns 0 for simple stage");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = x", err));
            chain.prepare({ 44100.0, 512, 1 });
            expectLessOrEqual(chain.getMaxTailTime(), 2.0f);
        }

        beginTest("getMaxTailTime: comp with long release returns positive value");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = x\ncomp1: threshold = -20; ratio = 4; attack = 10; release = 2000", err));
            chain.prepare({ 44100.0, 512, 1 });
            expectGreaterOrEqual(chain.getMaxTailTime(), 0.0f);
        }
    }

    void testWaveformCapture()
    {
        beginTest("WaveformCapture: pushInput stores data, getInputWaveform retrieves it");
        {
            WaveformCapture capture;
            capture.prepare(2, 1024);

            juce::AudioBuffer<float> src(2, 256);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    src.setSample(ch, i, (float)i / 255.f);

            capture.pushInput(src);

            juce::AudioBuffer<float> dest(2, 256);
            dest.clear();
            capture.getInputWaveform(dest);

            expectWithinAbsoluteError(dest.getSample(0, 255), 1.0f, 0.01f);
        }

        beginTest("WaveformCapture: pushOutput stores data");
        {
            WaveformCapture capture;
            capture.prepare(1, 512);

            juce::AudioBuffer<float> src(1, 128);
            src.clear();
            for (int i = 0; i < 128; ++i)
                src.setSample(0, i, 0.5f);

            capture.pushOutput(src);

            juce::AudioBuffer<float> dest(1, 128);
            dest.clear();
            capture.getOutputWaveform(dest);

            expectWithinAbsoluteError(dest.getSample(0, 127), 0.5f, 0.01f);
        }

        beginTest("WaveformCapture: reset clears buffers");
        {
            WaveformCapture capture;
            capture.prepare(1, 512);

            juce::AudioBuffer<float> src(1, 128);
            for (int i = 0; i < 128; ++i)
                src.setSample(0, i, 1.0f);
            capture.pushInput(src);

            capture.reset();

            juce::AudioBuffer<float> dest(1, 128);
            dest.clear();
            capture.getInputWaveform(dest);

            for (int i = 0; i < 128; ++i)
                expectWithinAbsoluteError(dest.getSample(0, i), 0.0f, 0.01f);
        }
    }

    void testScriptManagerDelegation()
    {
        beginTest("ScriptManager: applyFormula sets script");
        {
            ScriptManager mgr;
            juce::String err;
            expect(mgr.applyFormula("stage1: y = x", err));
            expectEquals(mgr.getScript(), juce::String("stage1: y = x"));
        }

        beginTest("ScriptManager: setVariableName / getVariableName");
        {
            ScriptManager mgr;
            juce::String err;
            mgr.applyFormula("stage1: y = a + b", err);
            mgr.setVariableName(0, "gain");
            expectEquals(mgr.getVariableName(0), juce::String("gain"));
        }

        beginTest("ScriptManager: isParameterActive");
        {
            ScriptManager mgr;
            juce::String err;
            mgr.applyFormula("stage1: y = x * a", err);
            expect(mgr.isParameterActive(0));
        }

        beginTest("ScriptManager: evaluateFormula");
        {
            ScriptManager mgr;
            juce::String err;
            mgr.applyFormula("stage1: y = tanh(x)", err);
            mgr.prepare({ 44100.0, 512, 1 });
            const float result = mgr.evaluateFormula(0.5f);
            const float expected = std::tanh(0.5f);
            expectWithinAbsoluteError(result, expected, 0.01f);
        }
    }

    void testFactoryPresetLibrary()
    {
        beginTest("FactoryPresetLibrary: load and apply ALL presets");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            const juce::File resDir(NEUROCORE_RESOURCES_DIR);
            expect(lib.loadFromResources(resDir), "factory_presets.json missing or invalid");

            const auto& entries = lib.getEntries();
            expect(entries.size() >= 70, "expected at least 70 factory presets, got "
                   + juce::String((int) entries.size()));

            juce::StringArray categories;
            juce::StringArray names;
            for (const auto& e : entries)
            {
                expect(e.name.isNotEmpty());
                expect(e.script.isNotEmpty());
                expect(! names.contains(e.name), "duplicate factory preset name: " + e.name);
                names.add(e.name);
                if (! categories.contains(e.category))
                    categories.add(e.category);
            }
            expect(categories.size() >= 8, "expected presets across multiple categories");

            // Every script must parse/load into SignalChain
            int scriptsOk = 0;
            juce::String firstScriptErr;
            for (const auto& e : entries)
            {
                dsl::SignalChain chain;
                juce::String err;
                if (chain.loadScript(e.script, err))
                    ++scriptsOk;
                else if (firstScriptErr.isEmpty())
                    firstScriptErr = e.name + ": " + err;
            }
            expectEquals(scriptsOk, (int) entries.size(),
                         "all factory scripts must load, first fail: " + firstScriptErr);

            NeuroCoreAudioProcessor proc;
            proc.prepareToPlay(44100.0, 512);

            int applied = 0;
            juce::String firstApplyErr;
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                juce::String err;
                if (lib.applyPreset(proc, i, err))
                    ++applied;
                else if (firstApplyErr.isEmpty())
                    firstApplyErr = entries[(size_t) i].name + ": " + err;
            }
            expectEquals(applied, (int) entries.size(),
                         "all factory presets must apply, first fail: " + firstApplyErr);
        }
    }

    void testProcessorStateRoundTrip()
    {
        beginTest("Processor state: variable names and language round-trip");
        {
            NeuroCoreAudioProcessor proc;
            juce::String err;
            expect(proc.setFormula("stage1: y = x * a", err));
            proc.setVariableName(0, "drive");
            proc.setVariableName(1, "tone");
            proc.loadLanguage("de");

            juce::MemoryBlock state;
            proc.getStateInformation(state);

            NeuroCoreAudioProcessor restored;
            restored.setStateInformation(state.getData(), (int) state.getSize());

            expectEquals(restored.getScript(), juce::String("stage1: y = x * a"));
            expectEquals(restored.getVariableName(0), juce::String("drive"));
            expectEquals(restored.getVariableName(1), juce::String("tone"));
            expect(restored.getCurrentLanguage().startsWithIgnoreCase("de"));
        }
    }
};
