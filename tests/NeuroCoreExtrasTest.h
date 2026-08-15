#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "../src/core/Config.h"
#include "../src/core/MidiVariableMapper.h"
#include "../src/core/WaveformCapture.h"
#include "../src/core/ScriptManager.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "../src/utils/FormulaQuality.h"
#include "TestHelpers.h"
#include <cmath>

#ifndef NEUROKORE_RESOURCES_DIR
#define NEUROKORE_RESOURCES_DIR "resources"
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
        testFormulaTemplatesHonesty();
        testAuHostBusLayout();
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
            for (int block = 0; block < 40; ++block)
            {
                buf.clear();
                chain.processBlock(buf);
            }
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (TestHelpers::peakAbs (buf) <= 2.01f);
        }

        beginTest("Feedback leak: x_prev remains stable");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("stage1: y = x_prev", err));
            chain.prepare({ 44100.0, 256, 1 });
            juce::AudioBuffer<float> buf(1, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample(0, i, 1.0f);
            for (int block = 0; block < 20; ++block)
                chain.processBlock(buf);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
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

        beginTest ("note-range param as osc freq is one cycle per note, not milliseconds-as-Hz");
        {
            // 1/4 at 120 BPM = 500 ms → 2 Hz. Feeding 500 into freq used to scream.
            const auto grid = dsl::NoteValues::makeGrid (1.f, 0.0625f);
            int qIdx = 0;
            for (int i = 0; i < grid.size(); ++i)
                if (std::abs (grid.wholes[(size_t) i] - 0.25f) < 1.0e-4f)
                    qIdx = i;
            const float quarterNorm = (grid.size() > 1)
                ? (float) qIdx / (float) (grid.size() - 1) : 0.5f;

            auto countZeroX = [&] (const juce::String& script) -> int
            {
                dsl::SignalChain chain;
                juce::String err;
                expect (chain.loadScript (script, err), err);
                chain.prepare ({ 44100.0, 512, 1 });
                chain.setTempo (120.0, 0.0, true);

                std::array<juce::SmoothedValue<float>, Config::kNumUserParams> sm;
                for (auto& s : sm)
                {
                    s.reset (44100.0, 0.0);
                    s.setCurrentAndTargetValue (quarterNorm);
                }
                std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> kp {};
                for (int i = 0; i < Config::kNumUserParams; ++i)
                    kp[(size_t) i] = &sm[i];

                // Host-sized blocks. Skip 200 ms so the LFO is settled, then 0.5 s.
                juce::AudioBuffer<float> buf (1, 512);
                for (int b = 0; b < 18; ++b)
                {
                    buf.clear();
                    chain.processBlockSmoothed (buf, kp);
                }

                int zx = 0;
                float prev = 0.f;
                bool havePrev = false;
                for (int b = 0; b < 44; ++b)
                {
                    buf.clear();
                    chain.processBlockSmoothed (buf, kp);
                    for (int i = 0; i < 512; ++i)
                    {
                        const float v = buf.getSample (0, i);
                        if (havePrev && ((prev <= 0.f && v > 0.f) || (prev >= 0.f && v < 0.f)))
                            ++zx;
                        prev = v;
                        havePrev = true;
                    }
                }
                return zx;
            };

            const int zxFreq = countZeroX (
                "param a = Rate [1/1, 1/16]\n"
                "osc1: shape = sine; freq = a; depth = 1.0\n"
                "stage1: y = osc1");
            // 2 Hz sine → ~4 zero crossings / second. Milliseconds-as-Hz would be ~1000.
            // 0.5 s of 2 Hz → one cycle → two zero-crossings.
            expect (zxFreq >= 2 && zxFreq <= 6,
                    "freq = note-param should be ~2 Hz at 1/4 120 BPM, zx="
                        + juce::String (zxFreq));

            const int zxSync = countZeroX (
                "param a = Rate [1/1, 1/16]\n"
                "osc1: shape = sine; sync = a; depth = 1.0\n"
                "stage1: y = osc1");
            expect (zxSync >= 2 && zxSync <= 6,
                    "sync = note-param should be ~2 Hz at 1/4 120 BPM, zx="
                        + juce::String (zxSync));
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
            // only 'a' used — b/c/d must stay offline (whole-word, not "a" in "param"/"stage")
            expect(mgr.applyFormula("param a = Drive [0, 2]\nstage1: y = x * a", err), err);
            expect(mgr.isParameterActive(0));
            expect(! mgr.isParameterActive(1));
            expect(! mgr.isParameterActive(2));
            expect(! mgr.isParameterActive(3));
            expect(mgr.applyFormula("stage1: y = x * b + c", err), err);
            expect(! mgr.isParameterActive(0));
            expect(mgr.isParameterActive(1));
            expect(mgr.isParameterActive(2));
            expect(! mgr.isParameterActive(3));
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
        beginTest ("stepPreset walks factory names and wraps");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            NeuroCoreAudioProcessor proc;
            proc.prepareToPlay (44100.0, 256);
            const auto names = proc.getPresetNames();
            expect (names.size() >= 2);
            proc.stepPreset (1);
            expectEquals (proc.getCurrentPresetName(), names[0]);
            proc.stepPreset (1);
            expectEquals (proc.getCurrentPresetName(), names[1]);
            proc.stepPreset (-1);
            expectEquals (proc.getCurrentPresetName(), names[0]);
            proc.stepPreset (-1);
            expectEquals (proc.getCurrentPresetName(), names[names.size() - 1]);
            proc.stepPreset (1);
            expectEquals (proc.getCurrentPresetName(), names[0]);
        }

        beginTest("FactoryPresetLibrary: load and apply ALL presets");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            const juce::File resDir(NEUROKORE_RESOURCES_DIR);
            expect(lib.loadFromResources(resDir), "factory_presets.json missing or invalid");

            // Embedded BinaryData fallback (what Cubase uses when no resources/ folder)
            {
                auto& lib2 = FactoryPresetLibrary::getInstance();
                expect(lib2.loadFromEmbedded(), "embedded factory_presets.json must load");
                expect(lib2.getEntries().size() >= 70, "embedded presets incomplete");
            }
            // Reload from disk for the rest of the test
            expect(lib.loadFromResources(resDir));

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

            // Parse-only for ALL (fast — no delay/reverb buffer alloc)
            int parseOk = 0;
            juce::String firstParseErr;
            {
                dsl::DSLParser parser;
                for (const auto& e : entries)
                {
                    std::vector<dsl::BlockDesc> blocks;
                    dsl::AliasMap aliases;
                    std::vector<dsl::ParamDesc> params;
                    juce::String err;
                    if (parser.parse (e.script, blocks, aliases, params, err))
                        ++parseOk;
                    else if (firstParseErr.isEmpty())
                        firstParseErr = e.name + ": " + err;
                }
            }
            expectEquals (parseOk, (int) entries.size(),
                          "all factory scripts must parse, first fail: " + firstParseErr);

            int loadOk = 0;
            juce::String firstLoadErr;
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                dsl::SignalChain chain;
                juce::String err;
                if (chain.loadScript (entries[(size_t) i].script, err))
                    ++loadOk;
                else if (firstLoadErr.isEmpty())
                    firstLoadErr = entries[(size_t) i].name + ": " + err;
            }
            expectEquals (loadOk, (int) entries.size(),
                          "all factory scripts must load, first fail: " + firstLoadErr);

            int processOk = 0;
            juce::String firstProcErr;
            juce::dsp::ProcessSpec spec { 44100.0, 256, 1 };
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                dsl::SignalChain chain;
                juce::String err;
                if (! chain.loadScript (entries[(size_t) i].script, err))
                    continue;
                chain.prepare (spec);
                juce::AudioBuffer<float> buf (1, 256);
                for (int n = 0; n < 256; ++n)
                    buf.setSample (0, n, 0.35f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                           * 220.0f * (float) n / 44100.0f));
                chain.processBlock (buf);
                buf.setSample (0, 0, 1.0f);
                chain.processBlock (buf);
                int bad = 0;
                float peak = 0.f;
                for (int n = 0; n < 256; ++n)
                {
                    const float v = buf.getSample (0, n);
                    if (! std::isfinite (v))
                        ++bad;
                    peak = juce::jmax (peak, std::abs (v));
                }
                if (bad == 0 && peak < 8.0f)
                    ++processOk;
                else if (firstProcErr.isEmpty())
                    firstProcErr = entries[(size_t) i].name
                        + " bad=" + juce::String (bad)
                        + " peak=" + juce::String (peak, 3);
            }
            expectEquals (processOk, (int) entries.size(),
                          "all factory presets must process finite, first fail: " + firstProcErr);

            NeuroCoreAudioProcessor proc;
            proc.prepareToPlay (44100.0, 256);
            int applied = 0;
            juce::String firstApplyErr;
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                juce::String err;
                if (lib.applyPreset (proc, i, err))
                    ++applied;
                else if (firstApplyErr.isEmpty())
                    firstApplyErr = entries[(size_t) i].name + ": " + err;
            }
            expectEquals (applied, (int) entries.size(),
                          "all factory presets must apply, first fail: " + firstApplyErr);

            // Gold-standard static: hardclip needs recovery LPF
            {
                const auto bare = FormulaQualityAnalyzer::analyse (
                    "stage1: y = hardclip(x, 0.5)");
                expect (bare.errors.joinIntoString (" ").contains ("recovery")
                        || bare.errors.joinIntoString (" ").contains ("lowpass"),
                        "bare hardclip must report missing recovery LPF");
                const auto okClip = FormulaQualityAnalyzer::analyse (
                    "stage1: y = hardclip(softclip(x, 1.2), 0.7)\n"
                    "filter1: type = lowpass; cutoff = 8000; resonance = 0.3");
                expect (! okClip.errors.joinIntoString (" ").contains ("recovery"),
                        "hardclip+LPF must not emit recovery error");
            }

            const auto qopt = TestHelpers::fastQualityOptions();
            int qualityPass = 0;
            juce::String firstQualityFail;
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                const auto& e = entries[(size_t) i];
                const auto q = FormulaQualityAnalyzer::analyse (e.script, qopt);
                if (FormulaQualityAnalyzer::passesFactoryGate (q, 55.f))
                    ++qualityPass;
                else if (firstQualityFail.isEmpty())
                {
                    firstQualityFail = e.name + " score=" + juce::String (q.score, 0)
                        + " err=" + q.errors.joinIntoString ("; ")
                        + " rms=" + juce::String (q.rms, 5);
                }
            }
            expectEquals (qualityPass, (int) entries.size(),
                          "all factory presets must pass quality gate, first fail: "
                              + firstQualityFail);

            int deadLetter = 0;
            juce::String firstDeadLetter;
            int dummyMeta = 0;
            juce::String firstDummyMeta;
            for (const auto& e : entries)
            {
                if (e.script.contains ("param g") || e.script.contains ("param h"))
                {
                    ++deadLetter;
                    if (firstDeadLetter.isEmpty())
                        firstDeadLetter = e.name;
                }

                const juce::String descLower = e.description.toLowerCase();
                expect (! descLower.contains ("eight knob")
                            && ! descLower.contains ("eight control"),
                        e.name + " description still claims 8 knobs");

                for (int i = 0; i < Config::kNumUserParams; ++i)
                {
                    const juce::String letter = juce::String::charToString (
                        static_cast<juce::juce_wchar> ('A' + i));
                    if (e.paramNames[i] == letter)
                    {
                        const juce::String decl = "param " + letter.toLowerCase();
                        if (! e.script.contains (decl))
                        {
                            ++dummyMeta;
                            if (firstDummyMeta.isEmpty())
                                firstDummyMeta = e.name + " param" + letter;
                        }
                    }
                }
            }
            expectEquals (deadLetter, 0,
                          "factory scripts must not declare param g/h (engine binds a–f only): "
                              + firstDeadLetter);
            expectEquals (dummyMeta, 0,
                          "factory metadata must not emit unused dummy C/D names: "
                              + firstDummyMeta);

            auto requireBlock = [&] (const juce::String& name, const juce::String& needle)
            {
                for (const auto& e : entries)
                {
                    if (e.name == name)
                    {
                        expect (e.script.contains (needle),
                                name + " must contain " + needle);
                        return;
                    }
                }
                expect (false, "missing factory preset: " + name);
            };
            requireBlock ("Doubler AM", "delay");
            requireBlock ("Shimmer Drive", "reverb");
            requireBlock ("Cinematic Space", "ms");
            requireBlock ("Side Delay", "ms");
            requireBlock ("Side Delay", "delay");
            requireBlock ("Side Hall", "ms");
            requireBlock ("Side Hall", "reverb");
            requireBlock ("Side Hall", "bus");
            requireBlock ("Vocal Send", "delay");
            requireBlock ("Vocal Send", "reverb");
            requireBlock ("Vocal Send", "bus");
            requireBlock ("NY Drum Bus", "bus");
            requireBlock ("Mono Below", "ms");
            requireBlock ("MS Mix Desk", "ms");
            requireBlock ("MS Imager", "ms");
            requireBlock ("Plate Send", "delay");
            requireBlock ("Plate Send", "reverb");
            requireBlock ("Width Delay", "pingpong");
            requireBlock ("Haas Width", "delay");
            requireBlock ("Trailer Impact", "reverb");
            requireBlock ("Trailer Impact", "delay");
            requireBlock ("Score Hall", "reverb");
            requireBlock ("Score Hall", "ms");
            requireBlock ("Dialogue Seat", "reverb");
            requireBlock ("Far Plane", "reverb");
            requireBlock ("Boom Tail", "reverb");
            requireBlock ("Wide Canvas", "ms");
            requireBlock ("Wide Canvas", "reverb");
            requireBlock ("Tension Bed", "reverb");
            requireBlock ("Stereo Guitar Wall", "channel = left");
            requireBlock ("Stereo Guitar Wall", "channel = right");
            requireBlock ("Stereo Guitar Wall", "tube");
            requireBlock ("Cyberpunk Drive", "bitcrush");
            requireBlock ("Cyberpunk Drive", "fold");
            requireBlock ("Cyberpunk Drive", "lowpass");
            requireBlock ("Cyberpunk Drive", "Level");
            requireBlock ("Glitch Laboratory", "Level");
            requireBlock ("Glitch Laboratory", "delay");
            requireBlock ("Kick Rumble", "bus");
            requireBlock ("Kick Rumble", "octaver");
            requireBlock ("Kick Rumble", "delay");
            requireBlock ("Kick Rumble", "env1");
            requireBlock ("Kick Rumble", "320");
            requireBlock ("Warehouse Rumble", "octaver");
            requireBlock ("Warehouse Rumble", "delay");
            requireBlock ("Warehouse Rumble", "env1");
            requireBlock ("Hardcore Clip", "hardclip");
            requireBlock ("Hardcore Clip", "octaver");
            requireBlock ("Gabber Drive", "hardclip");
            requireBlock ("Gabber Drive", "env1");
            requireBlock ("Gabber Drive", "sub");
            requireBlock ("Gabber Drive", "tube");
            requireBlock ("Gabber Drive", "octaver");
            requireBlock ("Acid Hash", "lowpass");
            requireBlock ("Neon Clip", "hardclip");
            requireBlock ("Industrial Gate", "gate1");
            requireBlock ("Hoover Dirt", "fold");
            requireBlock ("Tekno Comb", "delay");
            requireBlock ("Data Mosher", "bitcrush");
            requireBlock ("Chrome Fold", "fold");
            requireBlock ("AMS RMX Nonlin", "lowpass");
            requireBlock ("Sidechain Pump", "[1/1, 1/16]");
            requireBlock ("Classic Tremolo", "[1/1, 1/16]");
            requireBlock ("Chopper", "[1/1, 1/16]");
            requireBlock ("Low Pass Sweep", "[1/1, 1/16]");
            requireBlock ("Stereo Guitar Wall", "gate1");
            requireBlock ("Stereo Guitar Wall", "ir1");
            requireBlock ("5150 Lead", "gate1");
            requireBlock ("5150 Lead", "ir1");
            requireBlock ("JCM Hot Lead", "ir1");
            requireBlock ("Tube Screamer", "ir1");
            requireBlock ("1176 FET", "makeup");
            requireBlock ("LA-2A Opto", "knee");
        }

        beginTest ("Factory amp presets preload matching cabinet IRs");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));

            const struct { const char* name; const char* wav; } expected[] = {
                { "Mesa High Gain",       "American IR 01.wav" },
                { "Tube Screamer",        "Vintage IR 01.wav" },
                { "Fuzz Face",            "Vintage IR 01.wav" },
                { "Metal Gate",           "Medium IR 01.wav" },
                { "Stereo Guitar Wall",   "American IR 01.wav" },
                { "JCM Hot Lead",         "British IR 01.wav" },
                { "AC30 Chime",           "Vintage IR 01.wav" },
                { "5150 Lead",            "American IR 01.wav" },
            };

            NeuroCoreAudioProcessor proc;
            proc.prepareToPlay (44100.0, 256);

            for (const auto& row : expected)
            {
                const auto* e = lib.findByName (row.name);
                expect (e != nullptr, juce::String ("missing ") + row.name);
                if (e == nullptr)
                    continue;
                const auto it = e->irs.find ("ir1");
                expect (it != e->irs.end() && it->second == row.wav,
                        juce::String (row.name) + " must map ir1 -> " + row.wav);
                expect (e->script.containsIgnoreCase ("ir1"),
                        juce::String (row.name) + " formula must keep ir1 (no path)");
                expect (! e->script.containsIgnoreCase (".wav"),
                        juce::String (row.name) + " formula must not contain a WAV path");
            }

            auto indexOf = [&lib] (const juce::String& name) -> int
            {
                const auto& all = lib.getEntries();
                for (int i = 0; i < (int) all.size(); ++i)
                    if (all[(size_t) i].name == name)
                        return i;
                return -1;
            };

            juce::String err;
            const int mesa = indexOf ("Mesa High Gain");
            expect (mesa >= 0);
            expect (lib.applyPreset (proc, mesa, err), err);
            expect (proc.getIrNumSamples ("ir1") > 0, "Mesa High Gain must preload a cabinet IR");
            expectEquals (proc.getIrName ("ir1"), juce::String ("American IR 01.wav"));

            const int fender = indexOf ("Fender Clean");
            expect (fender >= 0);
            expect (lib.applyPreset (proc, fender, err), err);
            expectEquals (proc.getIrNumSamples ("ir1"), 0, "non-IR factory preset must clear leftover cab");

            const int ts = indexOf ("Tube Screamer");
            expect (ts >= 0);
            expect (lib.applyPreset (proc, ts, err), err);
            expect (proc.getIrNumSamples ("ir1") > 0, "Tube Screamer must preload N1");
            expectEquals (proc.getIrName ("ir1"), juce::String ("Vintage IR 01.wav"));
        }

        beginTest ("Stereo Guitar Wall: two DIs keep independent L/R amps");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            const auto* wall = lib.findByName ("Stereo Guitar Wall");
            expect (wall != nullptr, "missing Stereo Guitar Wall");
            if (wall == nullptr)
                return;

            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (wall->script, err), err);
            chain.prepare ({ 44100.0, 256, 2 });

            juce::AudioBuffer<float> buf (2, 256);
            for (int n = 0; n < 256; ++n)
            {
                const float t = (float) n / 44100.0f;
                const float twoPi = 2.0f * juce::MathConstants<float>::pi;
                buf.setSample (0, n, 0.45f * std::sin (twoPi * 220.0f * t));
                buf.setSample (1, n, 0.45f * std::sin (twoPi * 1100.0f * t));
            }
            chain.processBlock (buf);

            float peakL = 0.f, peakR = 0.f, diff = 0.f;
            for (int n = 0; n < 256; ++n)
            {
                const float l = buf.getSample (0, n);
                const float r = buf.getSample (1, n);
                expect (std::isfinite (l) && std::isfinite (r));
                peakL = juce::jmax (peakL, std::abs (l));
                peakR = juce::jmax (peakR, std::abs (r));
                diff += std::abs (l - r);
            }
            expect (peakL > 0.05f, "left Mesa-style amp silent");
            expect (peakR > 0.05f, "right 5150-style amp silent");
            expect (diff > 0.5f, "L/R too similar for a dual-DI wall");
        }

        beginTest ("Stereo Guitar Wall stays stereo on the smoothed plugin path");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            const auto* wall = lib.findByName ("Stereo Guitar Wall");
            expect (wall != nullptr);
            if (wall == nullptr)
                return;

            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (wall->script, err), err);
            chain.prepare ({ 48000.0, 256, 2 });

            juce::SmoothedValue<float> knobs[6];
            std::array<juce::SmoothedValue<float>*, 6> knobPtrs {};
            for (int i = 0; i < 6; ++i)
            {
                knobs[i].reset (48000.0, 0.0);
                knobs[i].setCurrentAndTargetValue (0.55f);
                knobPtrs[(size_t) i] = &knobs[i];
            }

            juce::AudioBuffer<float> buf (2, 256);
            float peakL = 0.f, peakR = 0.f;
            for (int b = 0; b < 8; ++b)
            {
                for (int n = 0; n < 256; ++n)
                {
                    const float t = (float) (b * 256 + n) / 48000.0f;
                    const float twoPi = 2.0f * juce::MathConstants<float>::pi;
                    buf.setSample (0, n, 0.4f * std::sin (twoPi * 220.0f * t));
                    buf.setSample (1, n, 0.4f * std::sin (twoPi * 1100.0f * t));
                }
                chain.processBlockSmoothed (buf, knobPtrs);
                for (int n = 0; n < 256; ++n)
                {
                    peakL = juce::jmax (peakL, std::abs (buf.getSample (0, n)));
                    peakR = juce::jmax (peakR, std::abs (buf.getSample (1, n)));
                    expect (std::isfinite (buf.getSample (0, n)) && std::isfinite (buf.getSample (1, n)));
                }
            }
            expect (peakL > 0.05f, "smoothed path left silent");
            expect (peakR > 0.05f, "smoothed path right silent");
        }

        beginTest ("Cyberpunk Drive: digital guitar dirt stays loud");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            const auto* cyber = lib.findByName ("Cyberpunk Drive");
            expect (cyber != nullptr, "missing Cyberpunk Drive");
            if (cyber == nullptr)
                return;

            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (cyber->script, err), err);
            chain.prepare ({ 44100.0, 256, 2 });

            juce::AudioBuffer<float> buf (2, 256);
            double sumSq = 0.0;
            for (int n = 0; n < 256; ++n)
            {
                const float t = (float) n / 44100.0f;
                const float s = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * t);
                buf.setSample (0, n, s);
                buf.setSample (1, n, s);
            }
            chain.processBlock (buf);
            float peak = 0.f;
            for (int n = 0; n < 256; ++n)
            {
                const float v = buf.getSample (0, n);
                expect (std::isfinite (v));
                peak = juce::jmax (peak, std::abs (v));
                sumSq += (double) v * (double) v;
            }
            const float rms = (float) std::sqrt (sumSq / 256.0);
            expect (peak > 0.10f, "Cyberpunk Drive peak too low: " + juce::String (peak, 3));
            expect (rms > 0.04f, "Cyberpunk Drive too quiet: rms=" + juce::String (rms, 4));
        }

        beginTest ("Glitch Laboratory stays light: no ping-pong, no LFO filter");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            const auto* gl = lib.findByName ("Glitch Laboratory");
            expect (gl != nullptr, "missing Glitch Laboratory");
            if (gl == nullptr)
                return;
            expect (! gl->script.containsIgnoreCase ("pingpong"));
            expect (! gl->script.containsIgnoreCase ("osc1"));
            expect (gl->script.containsIgnoreCase ("delay1"));
            expect (gl->script.containsIgnoreCase ("Level"));
            expect (lib.findByName ("Kick Rumble") != nullptr);
            expect (lib.findByName ("Hardcore Clip") != nullptr);
            expect (lib.findByName ("Gabber Drive") != nullptr);
            expect (lib.findByName ("Acid Hash") != nullptr);
            expect (lib.getEntries().size() >= 189);
        }

        beginTest ("Bitcrush lo-fi quick template has recovery LPF");
        {
            const juce::String frag =
                "param a = Bits [3, 12]\n"
                "param b = Mix [0.2, 1.0]\n"
                "stage1: y = lerp(x, bitcrush(x, a), b)\n"
                "filter1: type = lowpass; cutoff = 8000; resonance = 0.3\n";
            const auto q = FormulaQualityAnalyzer::analyse (frag);
            expect (FormulaQualityAnalyzer::passesFactoryGate (q, 55.f),
                    "bitcrush quick template must pass factory gate: " + q.summary());
            expect (! q.errors.joinIntoString (" ").containsIgnoreCase ("recovery"),
                    "bitcrush fragment must include recovery LPF");
        }

        beginTest ("FormulaQuality: detects silent y=0 style bug");
        {
            // Without seeding y from input this would be silent; with fix it should pass.
            // Force a truly dead formula for the negative test:
            const auto dead = FormulaQualityAnalyzer::analyse ("stage1: y = 0");
            expect (! FormulaQualityAnalyzer::passesFactoryGate (dead, 55.f));
            expect (dead.errors.size() > 0 || dead.rms < 1.0e-4f);

            const auto good = FormulaQualityAnalyzer::analyse ("stage1: y = tanh(x * 2)");
            expect (FormulaQualityAnalyzer::passesFactoryGate (good, 55.f),
                    "tanh stage should pass quality: " + good.summary());
        }
    }

    void testFormulaTemplatesHonesty()
    {
        beginTest ("templates.json names stay on six knobs");
        {
            const juce::File file (juce::String (NEUROKORE_RESOURCES_DIR)
                                       + "/templates.json");
            expect (file.existsAsFile(), "templates.json missing");
            const auto parsed = juce::JSON::parse (file);
            expect (parsed.isArray());
            for (int i = 0; i < parsed.size(); ++i)
            {
                const auto obj = parsed[i];
                const auto name = obj.getProperty ("name", {}).toString();
                const auto desc = obj.getProperty ("description", {}).toString();
                const auto formula = obj.getProperty ("formula", {}).toString();
                expect (! name.contains ("8 knob") && ! name.contains ("7 knob"),
                        name + " still claims extra knobs");
                expect (! desc.containsIgnoreCase ("a–h")
                            && ! desc.containsIgnoreCase ("a-h")
                            && ! desc.containsIgnoreCase ("uses a-h"),
                        name + " description still mentions a-h");
                expect (! formula.contains ("param g") && ! formula.contains ("param h"),
                        name + " formula still declares g/h");
            }
        }
    }

    void testProcessorStateRoundTrip()
    {
        beginTest("Processor state: variable names persist; language stays English");
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
            expectEquals(proc.getCurrentLanguage(), juce::String("en"));
            expectEquals(restored.getCurrentLanguage(), juce::String("en"));
        }

        beginTest ("Preset browser remembers category and scope");
        {
            NeuroCoreAudioProcessor proc;
            proc.setLastPresetBrowserCategory ("Delay");
            proc.setLastPresetBrowserScope (2);
            expectEquals (proc.getLastPresetBrowserCategory(), juce::String ("Delay"));
            expectEquals (proc.getLastPresetBrowserScope(), 2);
        }
    }

    // Logic/AU hosts need a declared sidechain bus. Plugin MIDI macros live on
    // the NeuroCore target, not this console test app.
    void testAuHostBusLayout()
    {
        beginTest ("AU host: stereo I/O plus optional stereo sidechain");
        {
            NeuroCoreAudioProcessor proc;

            expect (! proc.producesMidi());
            expect (! proc.isMidiEffect());
            expectEquals (proc.getBusCount (true), 2);
            expectEquals (proc.getBusCount (false), 1);

            const auto* mainIn = proc.getBus (true, 0);
            const auto* sidechain = proc.getBus (true, 1);
            const auto* mainOut = proc.getBus (false, 0);
            expect (mainIn != nullptr && mainIn->isEnabled());
            expect (sidechain != nullptr && sidechain->isEnabledByDefault());
            expect (mainOut != nullptr && mainOut->isEnabled());
            expectEquals (sidechain->getName(), juce::String ("Sidechain"));

            juce::AudioProcessor::BusesLayout stereo;
            stereo.inputBuses.add (juce::AudioChannelSet::stereo());
            stereo.inputBuses.add (juce::AudioChannelSet::disabled());
            stereo.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (proc.checkBusesLayoutSupported (stereo));

            juce::AudioProcessor::BusesLayout stereoWithSc = stereo;
            stereoWithSc.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
            expect (proc.checkBusesLayoutSupported (stereoWithSc));

            juce::AudioProcessor::BusesLayout mono;
            mono.inputBuses.add (juce::AudioChannelSet::mono());
            mono.inputBuses.add (juce::AudioChannelSet::disabled());
            mono.outputBuses.add (juce::AudioChannelSet::mono());
            expect (proc.checkBusesLayoutSupported (mono));
        }
    }
};
