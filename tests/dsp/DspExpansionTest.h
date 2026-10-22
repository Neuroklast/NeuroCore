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

        beginTest ("cached per-bus execution matches equivalent serial processing");
        {
            dsl::SignalChain serial, multi;
            serial.prepare ({ 48000.0, 128, 2 });
            multi.prepare ({ 48000.0, 128, 2 });
            juce::String err;
            expect (serial.loadScript ("stage1: y = x * 0.75", err), err);
            expect (multi.loadScript (
                "stage1: y = x * 0.5\n"
                "bus dirt:\n"
                "send: in = 1\n"
                "stage2: y = x * 0.25\n"
                "out: main = 1; dirt = 1\n", err), err);
            juce::AudioBuffer<float> first (2, 128), second (2, 128);
            for (int i = 0; i < 128; ++i)
            {
                const float x = 0.25f * std::sin (float (i) * 0.13f);
                first.setSample (0, i, x);
                first.setSample (1, i, -x);
            }
            second.makeCopyOf (first);
            serial.processBlock (first);
            multi.processBlock (second);
            float maxError = 0.f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 128; ++i)
                    maxError = juce::jmax (maxError,
                        std::abs (first.getSample (ch, i)
                                - second.getSample (ch, i)));
            expect (maxError < 1.0e-4f,
                    "per-bus compiled routing must agree with serial sum; err="
                    + juce::String (maxError, 7));
        }

        beginTest ("Pitch circular STFT buffers are independent of host block sizes");
        {
            auto render = [&] (const int blockSize)
            {
                dsl::SignalChain chain;
                chain.prepare ({ 48000.0, 512, 2 });
                juce::String err;
                expect (chain.loadScript (
                    "pitch1: semitones = 0; mix = 1; formant = 1; ceiling = 0", err), err);
                constexpr int total = 8192;
                std::vector<float> left, right;
                left.reserve (total);
                right.reserve (total);
                juce::AudioBuffer<float> audio (2, 512);
                for (int off = 0; off < total;)
                {
                    const int n = juce::jmin (blockSize, total - off);
                    juce::AudioBuffer<float> slice (audio.getArrayOfWritePointers(), 2, 0, n);
                    for (int i = 0; i < n; ++i)
                    {
                        const float t = (float) (off + i);
                        audio.setSample (0, i, 0.15f * std::sin (t * 0.117f));
                        audio.setSample (1, i, 0.11f * std::cos (t * 0.083f));
                    }
                    chain.processBlock (slice);
                    left.insert (left.end(), slice.getReadPointer (0),
                                 slice.getReadPointer (0) + n);
                    right.insert (right.end(), slice.getReadPointer (1),
                                  slice.getReadPointer (1) + n);
                    off += n;
                }
                return std::pair<std::vector<float>, std::vector<float>> {
                    std::move (left), std::move (right) };
            };
            const auto reference = render (128);
            for (const int chunk : { 37, 256, 511 })
            {
                const auto actual = render (chunk);
                float maxDeviation = 0.f;
                for (size_t i = 0; i < reference.first.size(); ++i)
                {
                    maxDeviation = juce::jmax (maxDeviation,
                        std::abs (reference.first[i] - actual.first[i]),
                        std::abs (reference.second[i] - actual.second[i]));
                    expect (std::isfinite (actual.first[i])
                         && std::isfinite (actual.second[i]));
                }
                expect (maxDeviation < 1.0e-4f,
                    "Pitch STFT must preserve continuous processing across "
                    + juce::String (chunk) + "-sample blocks; deviation="
                    + juce::String (maxDeviation, 7));
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

        beginTest ("ReverseDelay records a 250 ms window before reverse playback");
        {
            dsl::SignalChain chain;
            juce::String err;
            chain.prepare ({ 44100.0, 128, 1 });
            expect (chain.loadScript ("reversedelay1: time = 250; mix = 1; feedback = 0", err), err);

            constexpr int n = 25600; // 580 ms, enough for a complete reversal
            constexpr int window = 11025;
            juce::AudioBuffer<float> buffer (1, n);
            buffer.clear();
            buffer.setSample (0, window / 2, 1.f);
            juce::AudioBuffer<float> slice (1, 128);
            for (int off = 0; off < n; off += 128)
            {
                for (int i = 0; i < 128; ++i)
                    slice.setSample (0, i, buffer.getSample (0, off + i));
                chain.processBlock (slice);
                for (int i = 0; i < 128; ++i)
                    buffer.setSample (0, off + i, slice.getSample (0, i));
            }
            float beforeWindow = 0.f, afterWindow = 0.f;
            for (int i = 0; i < n; ++i)
            {
                const float a = std::abs (buffer.getSample (0, i));
                if (i < window) beforeWindow = juce::jmax (beforeWindow, a);
                else afterWindow = juce::jmax (afterWindow, a);
            }
            expect (beforeWindow < 1.0e-6f, "reverse audio must wait for a complete segment");
            expect (afterWindow > 0.25f, "recorded center pulse must play backwards");
            expect (TestHelpers::countNonFinite (buffer) == 0);
        }

        beginTest ("ReverseDelay reverses event order within a segment");
        {
            dsl::SignalChain chain;
            juce::String err;
            chain.prepare ({ 48000.0, 128, 1 });
            expect (chain.loadScript ("reversedelay1: time = 20; mix = 1; feedback = 0", err), err);
            constexpr int total = 3072;
            juce::AudioBuffer<float> audio (1, total);
            audio.clear();
            audio.setSample (0, 200, 0.35f);
            audio.setSample (0, 600, 0.8f);
            juce::AudioBuffer<float> slice (1, 128);
            for (int off = 0; off < total; off += 128)
            {
                for (int i = 0; i < 128; ++i)
                    slice.setSample (0, i, audio.getSample (0, off + i));
                chain.processBlock (slice);
                for (int i = 0; i < 128; ++i)
                    audio.setSample (0, off + i, slice.getSample (0, i));
            }
            float first = 0.f, second = 0.f;
            for (int i = 1250; i < 1400; ++i)
                first = juce::jmax (first, std::abs (audio.getSample (0, i)));
            for (int i = 1650; i < 1800; ++i)
                second = juce::jmax (second, std::abs (audio.getSample (0, i)));
            expect (first > 0.35f && second > 0.12f && first > second,
                "late/loud input must play before early/quiet input");
            expect (TestHelpers::countNonFinite (audio) == 0);
        }

        beginTest ("Amp gain smoothing stays logarithmic and respects host automation");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String err;
            expect (chain.loadScript ("amp1: gain = a * 18; output = 0; mix = 1", err), err);
            juce::AudioBuffer<float> buf (1, 128);
            auto settle = [&] (float v)
            {
                chain.setParameter (0, v);
                for (int k = 0; k < 64; ++k)
                {
                    for (int i = 0; i < 128; ++i)
                        buf.setSample (0, i, 0.08f);
                    chain.processBlock (buf);
                }
                return buf.getSample (0, 127);
            };
            const float low = settle (0.f);
            const float high = settle (1.f);
            expectWithinAbsoluteError (low, 0.08f, 0.005f);
            expectWithinAbsoluteError (
                high, 0.08f * juce::Decibels::decibelsToGain (18.f), 0.02f);
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

        beginTest ("Decimate rate=0.5 actually holds two-sample groups");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String err;
            expect (chain.loadScript (
                "decimate1: rate = 0.5; bits = 16; jitter = 0; hold = 0; "
                "recon = 0; mix = 1; output = 0", err), err);
            juce::AudioBuffer<float> audio (1, 128);
            for (int i = 0; i < 128; ++i)
                audio.setSample (0, i, (i & 1) ? 0.75f : 0.25f);
            chain.processBlock (audio);
            // Sample capture occurs at odd indices for an interval of 2.
            // An interval of 1 would leak the alternating even samples.
            for (int i = 8; i < 128; ++i)
                expectWithinAbsoluteError (audio.getSample (0, i),
                                           0.75f, 0.0001f);
        }

        beginTest ("Decimate all seven exposed parameters affect audio after host automation");
        {
            const char* knobs[] { "rate", "bits", "jitter", "hold", "recon", "mix", "output" };
            for (const auto* control : knobs)
            {
                auto render = [&] (float value)
                {
                    dsl::SignalChain chain;
                    chain.prepare ({ 48000.0, 256, 2 });
                    auto param = [&] (const char* id, const char* fallback)
                    {
                        return juce::String (id) + " = "
                             + (juce::String (control) == id ? juce::String ("a")
                                                          : juce::String (fallback)) + "; ";
                    };
                    juce::String err;
                    const juce::String script = "decimate1: "
                        + param ("rate", "0.85")
                        + param ("bits", "6")
                        + param ("jitter", "0")
                        + param ("hold", "0")
                        + param ("recon", "0.2")
                        + param ("mix", "1")
                        + param ("output", "0");
                    expect (chain.loadScript (script, err), err);
                    chain.setParameter (0, value);
                    juce::AudioBuffer<float> audio (2, 256);
                    std::vector<float> result;
                    for (int block = 0; block < 56; ++block)
                    {
                        for (int i = 0; i < 256; ++i)
                        {
                            const float x = (float) (block * 256 + i);
                            audio.setSample (0, i, 0.5f * std::sin (0.11f * x));
                            audio.setSample (1, i, 0.4f * std::cos (0.14f * x));
                        }
                        chain.processBlock (audio);
                        if (block >= 48)
                            result.insert (result.end(), audio.getReadPointer (0),
                                           audio.getReadPointer (0) + 256);
                    }
                    return result;
                };
                const auto low = render (0.f);
                const auto high = render (1.f);
                double difference = 0.;
                for (size_t i = 0; i < low.size(); ++i)
                    difference += std::abs (low[i] - high[i]);
                difference /= (double) juce::jmax ((size_t) 1, low.size());
                expect (difference > 0.001,
                    juce::String (control) + " automation has no measurable DSP effect: "
                    + juce::String (difference, 6));
            }
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

        beginTest ("Bitcrush dry bypass preserves signal before the global chain ceiling");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 2 });
            juce::String err;
            expect (chain.loadScript ("bitcrush1: bits = 2; tone = 800; mix = 0", err), err);
            juce::AudioBuffer<float> audio (2, 128);
            for (int i = 0; i < 128; ++i)
            {
                audio.setSample (0, i, 1.5f);
                audio.setSample (1, i, -1.4f);
            }
            chain.processBlock (audio);
            // SignalChain's post-bus output protection soft-limits floats
            // above unity using sign(x) * (2 - 1 / abs(x)). The Bitcrush
            // bypass must not impose its own additional [-1, 1] clamp.
            expectWithinAbsoluteError (audio.getSample (0, 127),
                                       2.f - 1.f / 1.5f, 1.0e-5f);
            expectWithinAbsoluteError (audio.getSample (1, 127),
                                      -(2.f - 1.f / 1.4f), 1.0e-5f);
        }

        beginTest ("Bitcrush tone automation changes spectral energy");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String err;
            expect (chain.loadScript ("bitcrush1: bits = 8; tone = a; mix = 1", err), err);
            juce::AudioBuffer<float> audio (1, 128);
            auto energy = [&] (float knob)
            {
                chain.setParameter (0, knob);
                double sum = 0.;
                for (int block = 0; block < 120; ++block)
                {
                    for (int i = 0; i < 128; ++i)
                        audio.setSample (0, i, 0.6f * std::sin (
                            (float) (block * 128 + i) * juce::MathConstants<float>::twoPi
                            * 6000.f / 48000.f));
                    chain.processBlock (audio);
                    if (block >= 112)
                        for (int i = 0; i < 128; ++i)
                            sum += audio.getSample (0, i) * audio.getSample (0, i);
                }
                return sum;
            };
            const double low = energy (0.f);
            const double high = energy (1.f);
            expect (high > low * 1.5,
                "higher cutoff must pass substantially more 6 kHz content");
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

        beginTest ("Shaper updates drive, output and mix from live automation");
        {
            auto renderAutomation = [&] (const juce::String& modulated, float input)
            {
                dsl::SignalChain chain;
                chain.prepare ({ 48000.0, 128, 1 });
                juce::String err;
                expect (chain.loadScript ("shaper1: mode = clip; drive = "
                    + (modulated == "drive" ? juce::String ("a * 24") : juce::String ("18"))
                    + "; output = " + (modulated == "output" ? juce::String ("a * -18") : juce::String ("0"))
                    + "; mix = " + (modulated == "mix" ? juce::String ("a") : juce::String ("1")), err), err);
                juce::AudioBuffer<float> buffer (1, 128);
                auto settle = [&] (float knob)
                {
                    chain.setParameter (0, knob);
                    for (int k = 0; k < 64; ++k)
                    {
                        for (int i = 0; i < 128; ++i)
                            buffer.setSample (0, i, input);
                        chain.processBlock (buffer);
                    }
                    return buffer.getSample (0, 127);
                };
                return std::pair<float, float> { settle (0.f), settle (1.f) };
            };

            const auto drive = renderAutomation ("drive", 0.4f);
            expect (drive.first < 0.6f && drive.second > 0.9f,
                    "automated drive must change clipping level");
            const auto trim = renderAutomation ("output", 0.4f);
            expect (trim.first > 0.9f && trim.second < 0.25f,
                    "automated output trim must change audible gain");
            const auto wet = renderAutomation ("mix", 0.4f);
            expectWithinAbsoluteError (wet.first, 0.4f, 0.01f);
            expect (wet.second > 0.9f, "automated dry/wet mix must engage the shaper");
        }

        beginTest ("Shaper fold alternates polarity after repeated reflections");
        {
            // Exercise the public audio path; Shaper is a private DSL node.
            // The old fmod(abs(x),2) incorrectly stayed positive beyond 2.
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String err;
            expect (chain.loadScript (
                "shaper1: mode = fold; drive = 0; output = 0; mix = 1", err), err);
            const float inputs[] { 0.5f, 1.5f, 2.5f, 3.5f, 4.5f, -2.5f, -3.5f };
            const float expected[] { 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
            juce::AudioBuffer<float> buffer (1, 128);
            for (size_t i = 0; i < sizeof (inputs) / sizeof (inputs[0]); ++i)
            {
                for (int sample = 0; sample < 128; ++sample)
                    buffer.setSample (0, sample, inputs[i]);
                chain.processBlock (buffer);
                expectWithinAbsoluteError (
                    buffer.getSample (0, 127), expected[i], 1.0e-5f);
            }
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

        beginTest ("Resonator cutoff automation changes bandpass response");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String err;
            expect (chain.loadScript (
                "resonator1: freq = 200 + a * 3800; res = 2; gain = 0; mix = 1", err), err);
            juce::AudioBuffer<float> audio (1, 128);
            auto energy = [&] (float knob)
            {
                chain.setParameter (0, knob);
                double sum = 0.;
                for (int block = 0; block < 100; ++block)
                {
                    for (int i = 0; i < 128; ++i)
                        audio.setSample (0, i, 0.2f * std::sin (
                            juce::MathConstants<float>::twoPi * 4000.f
                            * (float) (block * 128 + i) / 48000.f));
                    chain.processBlock (audio);
                    if (block >= 92)
                        for (int i = 0; i < 128; ++i)
                            sum += audio.getSample (0, i) * audio.getSample (0, i);
                }
                return sum;
            };
            const auto low = energy (0.f);
            const auto high = energy (1.f);
            expect (std::isfinite (low) && std::isfinite (high));
            expect (high > low * 1.5,
                "resonance peak must track live cutoff parameter");
        }

        beginTest ("Nonlinear filter cutoff automation remains effective");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String err;
            expect (chain.loadScript ("nlfilter1: cutoff = a; drive = 1; mix = 1", err), err);
            juce::AudioBuffer<float> audio (1, 128);
            auto measure = [&] (float value)
            {
                chain.setParameter (0, value);
                double energy = 0.;
                for (int block = 0; block < 100; ++block)
                {
                    for (int i = 0; i < 128; ++i)
                        audio.setSample (0, i, 0.3f * std::sin (
                            juce::MathConstants<float>::twoPi * 3000.f
                            * (float) (block * 128 + i) / 48000.f));
                    chain.processBlock (audio);
                    if (block >= 92)
                        for (int i = 0; i < 128; ++i)
                            energy += audio.getSample (0, i) * audio.getSample (0, i);
                }
                return energy;
            };
            const double low = measure (0.f);
            const double high = measure (1.f);
            expect (std::isfinite (low) && std::isfinite (high));
            expect (high > low * 3.0,
                "automated high cutoff must pass more energy than low cutoff");
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

        beginTest ("TapeStop depth=0 keeps the signal");
        {
            dsl::SignalChain chain;
            chain.prepare ({ 48000.0, 128, 1 });
            juce::String error;
            expect (chain.loadScript ("tapestop1: stop = 1; depth = 0; mix = 1", error), error);
            juce::AudioBuffer<float> buffer (1, 512);
            for (int i = 0; i < 512; ++i)
                buffer.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi
                                                         * 1000.0f * (float) i / 48000.0f));
            chain.processBlock (buffer);
            double sum = 0.0;
            for (int i = 32; i < 512; ++i)
            {
                const float y = buffer.getSample (0, i);
                sum += (double) y * y;
            }
            const float rms = (float) std::sqrt (sum / (512 - 32));
            expect (rms > 0.3f, "depth=0 must pass the signal, rms=" + juce::String (rms, 4));
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
