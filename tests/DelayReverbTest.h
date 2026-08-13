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

            // Process enough blocks so 10ms (~480 samples) can emerge
            for (int b = 0; b < 2; ++b)
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            // At mix=1 dry is gone; delayed impulse should appear near sample 480 of first block
            // After 2 blocks write advanced — check energy exists and first sample is near dry-less
            float energy = 0.f;
            for (int i = 0; i < buf.getNumSamples(); ++i)
                energy += std::abs (buf.getSample (0, i));
            expect (energy > 0.1f);
            expect (std::isfinite (energy));
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

        beginTest ("rhythmic gate delay style env+delay loads and stays finite");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            const char* script =
                "param a = Time [80, 480]\n"
                "param b = Feedback [0.15, 0.88]\n"
                "param c = Mix [0.2, 0.75]\n"
                "param d = Damp [800, 9000]\n"
                "param e = Duck [0.0, 0.9]\n"
                "param f = Drive [0.8, 4.0]\n"
                "env1: type = peak; attack = 0.008; release = 0.12\n"
                "delay1: time = a; feedback = b; mix = 1.0; damp = d\n"
                "stage1: y = softclip(x * (1.0 + f * 0.35), 1.15)\n"
                "stage2: y = lerp(x, y, c * (1.0 - env1 * e))";
            expect (chain.loadScript (script, err), err);
            // Rapid reload stress (message-thread style reconfigure)
            for (int r = 0; r < 8; ++r)
                expect (chain.loadScript (script, err), err);

            juce::AudioBuffer<float> buf (2, 256);
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
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.3f * std::sin (0.1f * (float) (b * 256 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.8f);
                }
                chain.processBlockSmoothed (buf, knobs);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
        }
    }
};
