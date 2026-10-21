#ifndef CLIPTEST_H
#define CLIPTEST_H

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/core/Config.h"
#include "../TestHelpers.h"

class ClipTest : public juce::UnitTest
{
public:
    ClipTest() : juce::UnitTest("ClipTest", "DSP") {}

    void runTest() override
    {
        beginTest("Parser akzeptiert gültigen Clipper-DSL");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("clip1: mode = soft; drive = 12; threshold = -6; knee = 0.5; bias = 0.1; outputTrim = -6; mix = 0.7; autoGain = on",
                                blocks, aliases, params, error));
            expect(error.isEmpty());
            expect(blocks.size() == 1);
            expect(blocks[0].type == "clip");
        }

        beginTest("Default-Konfiguration verarbeitet Stille stabil");
        {
            dsl::SignalChain chain;
            juce::String err;
            juce::dsp::ProcessSpec spec{ 44100.0, 128, 1 };
            chain.prepare(spec);
            expect(chain.loadScript("clip1: mode = hard; drive = 0; threshold = 0; mix = 1", err), err);
            expect(err.isEmpty());

            juce::AudioBuffer<float> buf(1, 128); // Stille
            buf.clear();
            chain.processBlock(buf);
            expect(TestHelpers::countNonFinite(buf) == 0);
            expect(TestHelpers::peakAbs(buf) == 0.0f);
        }

        beginTest("Hard-Clipper respektiert die Threshold exakt (Mono)");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("clip1: mode = hard; drive = 20; threshold = -6; mix = 1", err), err);
            juce::dsp::ProcessSpec spec{ 44100.0, 128, 1 };
            chain.prepare(spec);

            const float in = 0.9f; // lauter Eingangs-Peak
            juce::AudioBuffer<float> buf(1, 128);
            for (int i = 0; i < 128; ++i)
                buf.setSample(0, i, in);
            chain.processBlock(buf);

            const float thr = juce::Decibels::decibelsToGain(-6.0f);
            for (int i = 0; i < 128; ++i)
                expect(std::abs(buf.getSample(0, i)) <= thr + 1e-4f);
            expect(TestHelpers::countNonFinite(buf) == 0);
        }

        beginTest("Soft-Clipper bleibt unter der Threshold und ist weich");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("clip1: mode = soft; drive = 24; threshold = -12; knee = 1; mix = 1", err), err);
            juce::dsp::ProcessSpec spec{ 44100.0, 128, 1 };
            chain.prepare(spec);

            juce::AudioBuffer<float> buf(1, 128);
            for (int i = 0; i < 128; ++i)
                buf.setSample(0, i, 1.0f);
            chain.processBlock(buf);

            const float thr = juce::Decibels::decibelsToGain(-12.0f);
            for (int i = 0; i < 128; ++i)
                expect(std::abs(buf.getSample(0, i)) <= thr + 1e-4f);
            expect(TestHelpers::countNonFinite(buf) == 0);
        }

        beginTest("Stereo verhält sich identisch und bleibt endlich");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("clip1: mode = soft; drive = 18; threshold = -12; mix = 1", err), err);
            juce::dsp::ProcessSpec spec{ 48000.0, 256, 2 };
            chain.prepare(spec);

            juce::AudioBuffer<float> buf(2, 256);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    buf.setSample(ch, i, 0.7f * (ch == 0 ? 1.0f : -1.0f));
            chain.processBlock(buf);
            expect(TestHelpers::countNonFinite(buf) == 0);
        }

        beginTest("Sample-Rate-Wechsel und wiederholtes Prepare");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("clip1: mode = asymmetric; drive = 10; threshold = -3; bias = 0.2; mix = 0.5", err), err);
            for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                juce::dsp::ProcessSpec spec{ sr, 64, 2 };
                chain.prepare(spec);
                juce::AudioBuffer<float> buf(2, 64);
                buf.clear();
                for (int i = 0; i < 64; ++i)
                    buf.setSample(0, i, 0.5f);
                chain.processBlock(buf);
                expect(TestHelpers::countNonFinite(buf) == 0);
            }
        }

        beginTest("Wiederholte Verarbeitung bleibt stabil (keine NaN/Inf, kein Pumpen zu NaN)");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect(chain.loadScript("clip1: mode = hard; drive = 0; threshold = 0; mix = 1", err), err);
            juce::dsp::ProcessSpec spec{ 44100.0, 128, 1 };
            chain.prepare(spec);
            juce::AudioBuffer<float> buf(1, 128);
            for (int rep = 0; rep < 8; ++rep)
            {
                for (int i = 0; i < 128; ++i)
                    buf.setSample(0, i, 0.5f * static_cast<float>(rep % 2));
                chain.processBlock(buf);
                expect(TestHelpers::countNonFinite(buf) == 0);
            }
        }
    }
};

#endif // CLIPTEST_H
