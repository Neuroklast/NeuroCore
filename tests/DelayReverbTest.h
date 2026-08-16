#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/core/Config.h"
#include "TestHelpers.h"
#include <array>
#include <cmath>
#include <vector>

class DelayReverbTest : public juce::UnitTest
{
public:
    DelayReverbTest() : juce::UnitTest ("DelayReverbMs") {}

    void runTest() override
    {
        juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };

        beginTest ("delay impulse has one echo, not a periodic tick");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 10; feedback = 0.0; mix = 1.0; damp = 12000", err), err);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            int peaks = 0;
            float peakPos = 0.f, peakVal = 0.f;
            for (int i = 0; i < 512; ++i)
            {
                const float a = std::abs (buf.getSample (0, i));
                if (a > peakVal)
                {
                    peakVal = a;
                    peakPos = (float) i;
                }
                if (a > 0.15f)
                    ++peaks;
            }
            expect (peakVal > 0.3f);
            expect (peakPos > 400.f && peakPos < 520.f, "echo should sit near 10 ms (480 samples)");
            expect (peaks < 12, "Lagrange ringing / wrap ticks: too many peaks");
        }

        beginTest ("delay silence after one impulse has no wrap ticks");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 10; feedback = 0.0; mix = 1.0; damp = 12000", err), err);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            float laterPeak = 0.f;
            for (int b = 0; b < 8; ++b)
            {
                buf.clear();
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
                laterPeak = juce::jmax (laterPeak, TestHelpers::peakAbs (buf));
            }
            // fb=0: after the 10 ms echo the line is silence. A write-head wrap
            // would replay the impulse (~0.5+) every period.
            expect (laterPeak < 0.08f, "wrap/write-head tick after the echo, peak="
                    + juce::String (laterPeak, 4));
        }

        beginTest ("static filter cutoff jump stays finite");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "filter1: type = lowpass; cutoff = 400; resonance = 0.7", err), err);
            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                const float s = 0.4f * std::sin (i * 0.12f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expect (chain.loadScript (
                "filter1: type = lowpass; cutoff = 8000; resonance = 0.35", err), err);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (TestHelpers::peakAbs (buf) < 8.f);
        }

        beginTest ("eq freq sweep stays finite");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "param a = Freq [200, 8000]\n"
                "eq1: type = peak; freq = a; q = 1.4; gain = 6", err), err);
            juce::SmoothedValue<float> sm[Config::kNumUserParams];
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobs {};
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                sm[p].reset (48000.0, 0.01);
                sm[p].setCurrentAndTargetValue (0.2f);
                knobs[(size_t) p] = &sm[p];
            }
            juce::AudioBuffer<float> buf (2, 256);
            for (int b = 0; b < 12; ++b)
            {
                sm[0].setTargetValue (0.1f + 0.07f * (float) b);
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.3f * std::sin (i * 0.09f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, knobs);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
        }

        beginTest ("reverb plus delay long run stays finite");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 18; feedback = 0.45; mix = 0.35; damp = 6000\n"
                "reverb1: size = 0.55; decay = 0.6; damp = 0.4; mix = 0.4; width = 1",
                err), err);
            juce::AudioBuffer<float> buf (2, 256);
            for (int b = 0; b < 40; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = (b < 4) ? 0.5f * std::sin (i * 0.2f) : 0.f;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.8f);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
            expect (TestHelpers::peakAbs (buf) < 8.f);
        }

        beginTest ("delay block parses and delays impulse");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 10; feedback = 0.0; mix = 1.0; damp = 12000", err), err);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);

            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            // mix=1: dry is gone; the 10 ms echo sits near sample 480 of this block
            float energy = 0.f;
            for (int i = 0; i < buf.getNumSamples(); ++i)
                energy += std::abs (buf.getSample (0, i));
            expect (energy > 0.1f);
            expect (std::isfinite (energy));
            expect (std::abs (buf.getSample (0, 0)) < 0.05f);
        }

        beginTest ("eighth-note sync pingpong stays finite under tempo");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "stage1: y = softclip(x, 1.2)\n"
                "filter1: type = lowpass; cutoff = 9000; resonance = 0.28\n"
                "delay1: sync = 1/8; feedback = 0.45; mix = 0.4; damp = 5000; pingpong = true",
                err), err);
            chain.setTempo (120.0, 0.0, true);
            juce::AudioBuffer<float> buf (2, 256);
            for (int b = 0; b < 40; ++b)
            {
                // Simulate host tempo drift (common crackle trigger)
                chain.setTempo (118.0 + (b % 5), 0.0, true);
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.4f * std::sin (0.08f * (float) (b * 256 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.7f);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
            expect (TestHelpers::peakAbs (buf) < 8.f);
        }

        beginTest ("delay tempo sync loads");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: sync = 1/8; feedback = 0.3; mix = 0.4; damp = 5000; pingpong = true", err), err);
            chain.setTempo (120.0, 0.0, true);
            juce::AudioBuffer<float> buf (2, 256);
            buf.clear();
            for (int i = 0; i < 256; ++i)
            {
                buf.setSample (0, i, 0.3f * std::sin (i * 0.1f));
                buf.setSample (1, i, 0.3f * std::sin (i * 0.1f + 0.5f));
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expect (std::isfinite (buf.getSample (0, 100)));
            expect (chain.getMaxTailTime() > 0.05f);
        }

        beginTest ("reverb block produces wet energy and finite output");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "reverb1: size = 0.6; decay = 0.55; damp = 0.35; mix = 0.5; width = 1.0", err), err);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
            for (int b = 0; b < 8; ++b)
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (TestHelpers::peakAbs (buf) > 1.0e-4f);
            expect (chain.getMaxTailTime() > 0.5f);
        }

        beginTest ("reverb keeps stereo when fed L-only vs R-only");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "reverb1: size = 0.55; decay = 0.5; damp = 0.4; mix = 1.0; width = 1.0", err), err);

            juce::AudioBuffer<float> left (2, 512);
            left.clear();
            left.setSample (0, 0, 1.0f);
            for (int b = 0; b < 6; ++b)
                chain.processBlockSmoothed (left, TestHelpers::nullKnobs());
            const float lOnL = TestHelpers::peakAbs (left);

            chain.prepare (spec);
            expect (chain.loadScript (
                "reverb1: size = 0.55; decay = 0.5; damp = 0.4; mix = 1.0; width = 1.0", err), err);
            juce::AudioBuffer<float> right (2, 512);
            right.clear();
            right.setSample (1, 0, 1.0f);
            for (int b = 0; b < 6; ++b)
                chain.processBlockSmoothed (right, TestHelpers::nullKnobs());
            expect (lOnL > 1.0e-4f);
            expect (std::abs (right.getSample (1, 80)) + std::abs (right.getSample (1, 160)) > 0.f
                    || TestHelpers::peakAbs (right) > 1.0e-4f);
            expectEquals (TestHelpers::countNonFinite (left), 0);
            expectEquals (TestHelpers::countNonFinite (right), 0);
        }

        beginTest ("widen turns mono into L/R and keeps the mid");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "widen1: width = 0.85; delay = 16; bass = 120", err), err);
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                const float s = 0.4f * std::sin (i * 0.11f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            float maxDiff = 0.f, midErr = 0.f;
            for (int i = 64; i < 512; ++i)
            {
                const float l = buf.getSample (0, i);
                const float r = buf.getSample (1, i);
                const float src = 0.4f * std::sin (i * 0.11f);
                maxDiff = juce::jmax (maxDiff, std::abs (l - r));
                midErr = juce::jmax (midErr, std::abs (0.5f * (l + r) - src));
            }
            expect (maxDiff > 0.02f, "L and R must differ, diff=" + juce::String (maxDiff, 4));
            expect (midErr < 0.06f, "mono sum should stay the source, err=" + juce::String (midErr, 4));
        }

        beginTest ("ms encode/decode roundtrip");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "ms1: mode = encode\nms2: mode = decode", err), err);

            juce::AudioBuffer<float> buf (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 0.4f);
                buf.setSample (1, i, -0.2f);
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            for (int i = 0; i < 64; ++i)
            {
                expectWithinAbsoluteError (buf.getSample (0, i), 0.4f, 1.0e-4f);
                expectWithinAbsoluteError (buf.getSample (1, i), -0.2f, 1.0e-4f);
            }
        }

        beginTest ("ms mid process + side scale");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "ms1: mode = encode\n"
                "stage1: channel = mid; y = x * 0.5\n"
                "stage2: channel = side; y = x * 2.0\n"
                "ms2: mode = decode", err), err);

            juce::AudioBuffer<float> buf (2, 32);
            // L=1, R=1 → M=1, S=0 → mid*0.5=0.5, side*2=0 → L=R=0.5
            for (int i = 0; i < 32; ++i)
            {
                buf.setSample (0, i, 1.0f);
                buf.setSample (1, i, 1.0f);
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expectWithinAbsoluteError (buf.getSample (0, 10), 0.5f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (1, 10), 0.5f, 1.0e-3f);
        }

        beginTest ("filter channel=side after ms encode");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "ms1: mode = encode\n"
                "filter1: type = highpass; cutoff = 500; resonance = 0.4; channel = side\n"
                "ms2: mode = decode", err), err);
            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                buf.setSample (0, i, 0.5f);
                buf.setSample (1, i, 0.1f);
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expectEquals (TestHelpers::countNonFinite (buf), 0);
        }

        beginTest ("factory-style delay+drive script loads");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            const char* script =
                "param a = Time [70, 150]\n"
                "param b = Feedback [0.0, 0.35]\n"
                "param c = Mix [0.15, 0.5]\n"
                "param d = Damp [2500, 12000]\n"
                "stage1: y = softclip(x, 1.08)\n"
                "delay1: time = a; feedback = b; mix = c; damp = d";
            expect (chain.loadScript (script, err), err);
            juce::AudioBuffer<float> buf (2, 128);
            for (int i = 0; i < 128; ++i)
                buf.setSample (0, i, 0.2f * std::sin (i * 0.2f));
            buf.copyFrom (1, 0, buf, 0, 0, 128);
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobs {};
            chain.processBlockSmoothed (buf, knobs);
            expect (std::isfinite (buf.getRMSLevel (0, 0, 128)));
        }

        beginTest ("rhythmic gate delay keeps dry and does not replace the input");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            const char* script =
                "param a = Time [80, 480]\n"
                "param b = Feedback [0.12, 0.72]\n"
                "param c = Mix [0.15, 0.7]\n"
                "param d = Damp [800, 9000]\n"
                "param e = Duck [0.0, 0.85]\n"
                "param f = Drive [0.8, 2.8]\n"
                "env1: type = peak; attack = 0.006; release = 0.16\n"
                "stage1: y = x\n"
                "bus echo:\n"
                "  send: in = 1\n"
                "  delay1: time = a; feedback = b; mix = 1; damp = d\n"
                "  stage2: y = softclip(x * (1.0 + f * 0.22), 1.08)\n"
                "  stage3: y = x * (1.0 - env1 * e)\n"
                "out: main = 1; echo = c";
            expect (chain.loadScript (script, err), err);
            expect (! juce::String (script).contains ("lerp(x, y"));

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            // Dry must survive sample 0. mix=1 delay + lerp(x,y) used to wipe it.
            expect (std::abs (buf.getSample (0, 0)) > 0.5f);
            expectEquals (TestHelpers::countNonFinite (buf), 0);

            for (int r = 0; r < 6; ++r)
                expect (chain.loadScript (script, err), err);

            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobs {};
            juce::SmoothedValue<float> sm[Config::kNumUserParams];
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                sm[p].reset (48000.0, 0.01);
                sm[p].setCurrentAndTargetValue (0.5f);
                knobs[(size_t) p] = &sm[p];
            }
            for (int b = 0; b < 20; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = 0.3f * std::sin (0.1f * (float) (b * 512 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.8f);
                }
                chain.processBlockSmoothed (buf, knobs);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
        }

        beginTest ("delay at 8x sample rate has one echo and no wrap ticks");
        {
            // Oversampled delay lines are sized at OS SR. A 1×-sized ring
            // wraps every 2s/factor and ticks. This is the OS-rate chain alone.
            juce::dsp::ProcessSpec os { 48000.0 * 8.0, 512, 2 };
            dsl::SignalChain chain;
            chain.prepare (os);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 40; feedback = 0.0; mix = 1.0; damp = 12000", err), err);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            const float echoAt = 0.040f * (float) os.sampleRate; // 15360
            float laterPeak = 0.f;
            int extraPeaks = 0;
            int sample = 512;
            for (int b = 0; b < 80; ++b)
            {
                buf.clear();
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
                for (int i = 0; i < 512; ++i, ++sample)
                {
                    const float a = std::abs (buf.getSample (0, i));
                    const bool nearEcho = std::abs ((float) sample - echoAt) < 64.f;
                    if (nearEcho)
                        continue;
                    laterPeak = juce::jmax (laterPeak, a);
                    if (a > 0.08f)
                        ++extraPeaks;
                }
            }
            expect (laterPeak < 0.06f, "8x-rate wrap/write-head tick, peak="
                    + juce::String (laterPeak, 4) + " extras=" + juce::String (extraPeaks));
        }

        beginTest ("wow-modulated delay has no block-rate clicks");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "osc1: shape = sine; freq = 0.65; depth = 1.0\n"
                "delay1: time = 180 + osc1 * 8; feedback = 0.42; mix = 0.45; damp = 2600",
                err), err);

            juce::AudioBuffer<float> buf (2, 128);
            float prev = 0.f;
            bool have = false;
            float maxBoundary = 0.f, maxInterior = 0.f;
            for (int b = 0; b < 80; ++b)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float t = (float) (b * 128 + i) / 48000.f;
                    const float s = 0.35f * std::sin (2.f * juce::MathConstants<float>::pi * 220.f * t);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
                if (b < 16)
                {
                    prev = buf.getSample (0, 127);
                    have = true;
                    continue;
                }
                for (int i = 0; i < 128; ++i)
                {
                    const float y = buf.getSample (0, i);
                    if (have)
                    {
                        const float jump = std::abs (y - prev);
                        if (i == 0)
                            maxBoundary = juce::jmax (maxBoundary, jump);
                        else
                            maxInterior = juce::jmax (maxInterior, jump);
                    }
                    prev = y;
                    have = true;
                }
            }
            expect (maxBoundary < maxInterior * 2.5f + 0.02f,
                    "wow delay block click: boundary=" + juce::String (maxBoundary, 4)
                    + " interior=" + juce::String (maxInterior, 4));
        }
    }
};
