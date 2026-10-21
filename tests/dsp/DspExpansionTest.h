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

        beginTest ("Shaper mode=clip hard-clips while mode=fold folds");
        {
            auto render = [&] (const char* mode, float input)
            {
                dsl::SignalChain chain;
                chain.prepare ({ 48000.0, 128, 1 });
                juce::String error;
                expect (chain.loadScript (juce::String ("shaper1: mode = ") + mode
                                          + "; drive = 12; outputTrim = 0; mix = 1", error), error);
                juce::AudioBuffer<float> buffer (1, 128);
                buffer.clear();
                buffer.setSample (0, 0, input);
                chain.processBlock (buffer);
                return buffer.getSample (0, 0);
            };

            // 0.5 at +12 dB is about 1.99: clip holds 1, fold wraps back to about 0.01.
            const float clipped = render ("clip", 0.5f);
            const float folded  = render ("fold", 0.5f);
            expectWithinAbsoluteError (clipped, 1.0f, 1.0e-3f);
            expect (std::abs (folded) < 0.1f, "fold must wrap back, got " + juce::String (folded, 5));
            expect (std::abs (render ("symmetric", 0.5f) - folded) < 1.0e-6f,
                    "symmetric stays a fold alias");
        }

        beginTest ("WaveFold keeps folding beyond one reflection");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String error;
            expect (chain.loadScript ("wavefold1: drive = 1; fold = 1; mix = 1", error), error);
            juce::AudioBuffer<float> buffer (1, 128);
            buffer.clear();
            buffer.setSample (0, 0, 3.5f);
            chain.processBlock (buffer);
            // Triangle fold of 3.5 at fold=1 is -0.5; a single reflection would leave -1.5.
            expectWithinAbsoluteError (buffer.getSample (0, 0), -0.5f, 1.0e-4f);
        }

        beginTest ("ReverseDelay reports its feedback tail");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String error;
            expect (chain.loadScript ("reversedelay1: time = 250; feedback = 0.5; mix = 1", error), error);
            const float tail = chain.getMaxTailTime();
            expect (tail > 0.25f, "tail must cover the delay time, got " + juce::String (tail, 4));
            expect (tail < 12.1f, "tail must stay clamped, got " + juce::String (tail, 4));
        }

        beginTest ("ReverseDelay keeps stereo channels separate");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 2 });
            juce::String error;
            expect (chain.loadScript ("reversedelay1: time = 10; feedback = 0; mix = 1", error), error);
            juce::AudioBuffer<float> buffer (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                buffer.setSample (0, i, 0.5f);
                buffer.setSample (1, i, 0.0f);
            }
            chain.processBlock (buffer);
            float peakL = 0.f, peakR = 0.f;
            for (int i = 0; i < 512; ++i)
            {
                peakL = juce::jmax (peakL, std::abs (buffer.getSample (0, i)));
                peakR = juce::jmax (peakR, std::abs (buffer.getSample (1, i)));
            }
            expect (peakL > 0.1f, "left must carry the delay, got " + juce::String (peakL, 4));
            expect (peakR < 1.0e-6f, "right must stay silent, got " + juce::String (peakR, 4));
        }

        beginTest ("TapeStop keeps stereo channels separate");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 2 });
            juce::String error;
            expect (chain.loadScript ("tapestop1: stop = 1; depth = 0.5; mix = 1", error), error);
            juce::AudioBuffer<float> buffer (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                buffer.setSample (0, i, 0.5f);
                buffer.setSample (1, i, 0.0f);
            }
            chain.processBlock (buffer);
            float peakL = 0.f, peakR = 0.f;
            for (int i = 0; i < 512; ++i)
            {
                peakL = juce::jmax (peakL, std::abs (buffer.getSample (0, i)));
                peakR = juce::jmax (peakR, std::abs (buffer.getSample (1, i)));
            }
            expect (peakL > 0.1f, "left must carry the delay, got " + juce::String (peakL, 4));
            expect (peakR < 1.0e-6f, "right must stay silent, got " + juce::String (peakR, 4));
        }

        beginTest ("NlFilter cutoff follows the one-pole response");
        {
            auto renderRms = [&] (float freq)
            {
                dsl::SignalChain chain;
                chain.prepare ({ 48000.0, 256, 1 });
                juce::String error;
                expect (chain.loadScript ("nlfilter1: cutoff = 1000; drive = 1; mix = 1", error), error);
                constexpr int total = 48000;
                juce::AudioBuffer<float> buffer (1, 256);
                double sum = 0.0;
                for (int off = 0; off < total; off += 256)
                {
                    for (int i = 0; i < 256; ++i)
                        buffer.setSample (0, i, 0.01f * std::sin (juce::MathConstants<float>::twoPi
                                                                  * freq * (float) (off + i) / 48000.0f));
                    chain.processBlock (buffer);
                    if (off >= total / 2)
                        for (int i = 0; i < 256; ++i)
                        {
                            const float y = buffer.getSample (0, i);
                            sum += (double) y * y;
                        }
                }
                return (float) std::sqrt (sum / (total / 2.0));
            };

            const float low = renderRms (100.0f);
            const float atCutoff = renderRms (1000.0f);
            expect (low > 0.005f, "passband must pass the signal, rms=" + juce::String (low, 5));
            const float ratio = atCutoff / juce::jmax (1.0e-9f, low);
            expectWithinAbsoluteError (ratio, 0.7071f, 0.12f);
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
