#ifndef DSPEXPANSIONTEST_H
#define DSPEXPANSIONTEST_H

#include <JuceHeader.h>
#include "../../src/dsl/SignalChain.h"
#include "../TestHelpers.h"

class DspExpansionTest : public juce::UnitTest
{
public:
    DspExpansionTest() : juce::UnitTest ("DspExpansionTest", "DSP") {}

    void runTest() override
    {
        beginTest ("Saturator tone, dynamics, character and outputTrim reach native DSP");
        {
            auto render = [&] (const juce::String& args)
            {
                dsl::SignalChain chain;
                chain.prepare ({ 48000.0, 128, 2 });
                juce::String error;
                expect (chain.loadScript ("saturate1: mode = tube; drive = 12; mix = 1; " + args, error), error);
                std::vector<float> samples;
                juce::AudioBuffer<float> buffer (2, 128);
                for (int block = 0; block < 64; ++block)
                {
                    for (int i = 0; i < 128; ++i)
                    {
                        buffer.setSample (0, i, 0.7f * std::sin (float (block * 128 + i) * 0.4f));
                        buffer.setSample (1, i, 0.f);
                    }
                    chain.processBlock (buffer);
                    samples.insert (samples.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 128);
                }
                return samples;
            };
            const auto baseline = render ("tone = 0.5; dynamics = 0.5; character = 0.5; outputTrim = 0");
            for (const auto* changed : { "tone = 1", "dynamics = 0", "character = 1", "outputTrim = -6" })
            {
                const auto samples = render (changed);
                double difference = 0.;
                for (size_t i = 0; i < samples.size(); ++i) difference += std::abs (samples[i] - baseline[i]);
                expect (difference / samples.size() > 0.001, juce::String (changed) + " must change wet audio");
            }
        }

        beginTest ("Frequency shifter zero shift is a latency-aligned identity");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String error;
            expect (chain.loadScript ("freqshift1: hz = 0; mix = 0.5", error), error);
            expectEquals (chain.getIrLatencySamples(), 64);
            expect (chain.getMaxTailTime() >= 128.f / 48000.f);
            juce::AudioBuffer<float> buffer (1, 128);
            buffer.clear();
            buffer.setSample (0, 0, 1.f);
            chain.processBlock (buffer);
            float errorPeak = 0.f;
            for (int i = 0; i < 128; ++i)
                errorPeak = juce::jmax (errorPeak, std::abs (buffer.getSample (0, i) - (i == 64 ? 1.f : 0.f)));
            expect (errorPeak < 1e-6f, "dry and real paths must share the reported latency");
        }

        beginTest ("ReverseDelay time=250 is 250 ms, not 2 s");
        {
            dsl::SignalChain chain;
            juce::String err;
            juce::dsp::ProcessSpec spec { 44100.0, 128, 1 };
            chain.prepare (spec);
            expect (chain.loadScript ("reversedelay1: time = 250; mix = 1; feedback = 0", err), err);

            constexpr int n = 12800;
            juce::AudioBuffer<float> buf (1, n);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            juce::AudioBuffer<float> slice (1, 128);
            for (int off = 0; off < n; off += 128)
            {
                for (int i = 0; i < 128; ++i)
                    slice.setSample (0, i, buf.getSample (0, off + i));
                chain.processBlock (slice);
                for (int i = 0; i < 128; ++i)
                    buf.setSample (0, off + i, slice.getSample (0, i));
            }

            const float peak = TestHelpers::peakAbs (buf);
            expect (peak > 0.25f, "250 ms wet should appear inside 290 ms, peak="
                    + juce::String (peak, 4));
            expect (TestHelpers::countNonFinite (buf) == 0);
        }

        beginTest ("Decimate stereo hold uses L, not R");
        {
            dsl::SignalChain chain;
            juce::String err;
            juce::dsp::ProcessSpec spec { 44100.0, 64, 2 };
            chain.prepare (spec);
            expect (chain.loadScript ("decimate1: rate = 0.5; bits = 16; recon = 0; mix = 1", err), err);

            juce::AudioBuffer<float> buf (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 1.0f);
                buf.setSample (1, i, 0.0f);
            }
            chain.processBlock (buf);

            const float peakL = std::abs (buf.getSample (0, 63));
            expect (peakL > 0.5f, "L should stay held near 1, peakL="
                    + juce::String (peakL, 4));
            expect (TestHelpers::countNonFinite (buf) == 0);
        }

        beginTest ("Saturate drive=0 mix=0 passes input unchanged");
        {
            dsl::SignalChain chain;
            juce::String err;
            juce::dsp::ProcessSpec spec { 44100.0, 128, 1 };
            chain.prepare (spec);
            expect (chain.loadScript ("saturate1: mode = tube; drive = 0; bias = 0; outputTrim = 0; mix = 0; autoGain = off", err), err);

            juce::AudioBuffer<float> buf (1, 128);
            for (int i = 0; i < 128; ++i)
                buf.setSample (0, i, 0.5f * std::sin (float (i) * 0.1f));
            chain.processBlock (buf);

            bool ok = true;
            for (int i = 0; i < 128; ++i)
                if (std::abs (buf.getSample (0, i) - 0.5f * std::sin (float (i) * 0.1f)) > 1e-4f) { ok = false; break; }
            expect (ok, "mix=0 should pass dry through unchanged");
            expect (TestHelpers::countNonFinite (buf) == 0);
        }

        beginTest ("Shaper drive=0 mix=0 passes input unchanged");
        {
            dsl::SignalChain chain;
            juce::String err;
            juce::dsp::ProcessSpec spec { 44100.0, 128, 1 };
            chain.prepare (spec);
            expect (chain.loadScript ("shaper1: mode = fold; drive = 0; outputTrim = 0; mix = 0", err), err);

            juce::AudioBuffer<float> buf (1, 128);
            for (int i = 0; i < 128; ++i)
                buf.setSample (0, i, 0.5f * std::sin (float (i) * 0.1f));
            chain.processBlock (buf);

            bool ok = true;
            for (int i = 0; i < 128; ++i)
                if (std::abs (buf.getSample (0, i) - 0.5f * std::sin (float (i) * 0.1f)) > 1e-4f) { ok = false; break; }
            expect (ok, "mix=0 should pass dry through unchanged");
            expect (TestHelpers::countNonFinite (buf) == 0);
        }

        beginTest ("TapeStop stays finite under repeated load");
        {
            dsl::SignalChain chain;
            juce::String err;
            juce::dsp::ProcessSpec spec { 44100.0, 128, 1 };
            chain.prepare (spec);
            expect (chain.loadScript ("tapestop1: stop = 1; depth = 1; mix = 1", err), err);

            juce::AudioBuffer<float> buf (1, 128);
            buf.setSample (0, 0, 1.0f);
            for (int i = 1; i < 128; ++i)
                buf.setSample (0, i, 0.0f);
            for (int r = 0; r < 100; ++r)
                chain.processBlock (buf);
            expect (TestHelpers::countNonFinite (buf) == 0, "TapeStop must not produce NaN/Inf");
        }
    }
};

#endif
