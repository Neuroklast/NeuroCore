#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsp/OutputSanitizer.h"
#include "../src/dsp/LatencyAlignedSidechain.h"
#include "../src/utils/ExpressionEvaluator.h"
#include "../src/core/PluginProcessor.h"
#include "TestHelpers.h"
#include <cmath>
#include <vector>

/** Regression tests for known crackle root causes. */
class CrackleFixesTest : public juce::UnitTest
{
public:
    CrackleFixesTest() : juce::UnitTest ("CrackleFixes") {}

    void runTest() override
    {
        juce::dsp::ProcessSpec spec { 48000.0, 256, 2 };

        beginTest ("ADAA continuous across block boundaries (no reset click)");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript ("stage1: y = softclip(x, 4.0)", err), err);

            // Steady sine across many blocks — max |Δsample| should stay modest
            float phase = 0.f;
            const float dphi = 2.f * juce::MathConstants<float>::pi * 440.f / 48000.f;
            float maxJump = 0.f;
            float last = 0.f;
            bool haveLast = false;

            int bad = 0;
            for (int b = 0; b < 16; ++b)
            {
                juce::AudioBuffer<float> buf (2, 128);
                for (int i = 0; i < 128; ++i)
                {
                    const float s = 0.6f * std::sin (phase);
                    phase += dphi;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 128; ++i)
                {
                    const float y = buf.getSample (0, i);
                    if (haveLast)
                        maxJump = juce::jmax (maxJump, std::abs (y - last));
                    last = y;
                    haveLast = true;
                }
            }
            expectEquals (bad, 0);
            expect (maxJump < 0.25f);
        }

        beginTest ("reverb size change does not inject a hard zero click");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            // size driven by constant — we reload with different size
            expect (chain.loadScript (
                "reverb1: size = 0.3; decay = 0.5; damp = 0.4; mix = 0.8; width = 1.0", err), err);

            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                buf.setSample (0, i, (i == 0) ? 0.8f : 0.f);
                buf.setSample (1, i, (i == 0) ? 0.8f : 0.f);
            }
            for (int b = 0; b < 6; ++b)
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            // Change size mid-stream via new script — lengths change but no buffer clear
            expect (chain.loadScript (
                "reverb1: size = 0.9; decay = 0.5; damp = 0.4; mix = 0.8; width = 1.0", err), err);

            float maxAbs = 0.f;
            int nanCount = 0;
            for (int b = 0; b < 4; ++b)
            {
                // keep feeding silence after impulse energy is in the tanks
                for (int i = 0; i < 512; ++i)
                {
                    buf.setSample (0, i, 0.f);
                    buf.setSample (1, i, 0.f);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                for (int i = 0; i < 512; ++i)
                {
                    const float y = buf.getSample (0, i);
                    if (! std::isfinite (y)) ++nanCount;
                    maxAbs = juce::jmax (maxAbs, std::abs (y));
                }
            }
            expect (nanCount == 0);
            // After size change, output must stay finite (no NaN explosion)
            expect (maxAbs < 2.0f);
        }

        beginTest ("sanitizer keeps decaying wet tails when dry is silent");
        {
            OutputSanitizer san;
            juce::dsp::ProcessSpec ps { 48000.0, 256, 2 };
            san.prepare (ps);

            juce::AudioBuffer<float> dry (2, 256);
            juce::AudioBuffer<float> wet (2, 256);
            dry.clear();

            // Prime with loud wet while dry silent briefly
            for (int i = 0; i < 256; ++i)
            {
                wet.setSample (0, i, 0.3f);
                wet.setSample (1, i, 0.3f);
            }
            for (int b = 0; b < 3; ++b)
            {
                auto dryB = juce::dsp::AudioBlock<const float> (dry);
                auto wetB = juce::dsp::AudioBlock<float> (wet);
                san.process (dryB, wetB);
            }

            // Decaying tail (each block quieter) — must stay audible
            float level = 0.25f;
            float peak = 0.f;
            for (int b = 0; b < 12; ++b)
            {
                level *= 0.85f;
                for (int i = 0; i < 256; ++i)
                {
                    wet.setSample (0, i, level);
                    wet.setSample (1, i, level);
                }
                auto dryB = juce::dsp::AudioBlock<const float> (dry);
                auto wetB = juce::dsp::AudioBlock<float> (wet);
                san.process (dryB, wetB);
                for (int i = 0; i < 256; ++i)
                    peak = juce::jmax (peak, std::abs (wet.getSample (0, i)));
            }
            expect (peak > 0.02f);
        }

        beginTest ("sanitizer preserves wet when dry silent (musical tails)");
        {
            // Architecture: sanitizer only mutes residual noise floor, not sustain.
            OutputSanitizer san;
            juce::dsp::ProcessSpec ps { 48000.0, 256, 2 };
            san.prepare (ps);

            juce::AudioBuffer<float> dry (2, 256);
            juce::AudioBuffer<float> wet (2, 256);
            dry.clear();
            for (int i = 0; i < 256; ++i)
            {
                wet.setSample (0, i, 0.3f);
                wet.setSample (1, i, 0.3f);
            }
            for (int b = 0; b < 20; ++b)
            {
                auto dryB = juce::dsp::AudioBlock<const float> (dry);
                auto wetB = juce::dsp::AudioBlock<float> (wet);
                san.process (dryB, wetB);
            }
            float peak = 0.f;
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (wet.getSample (0, i)));
            expect (peak > 0.1f);
        }

        beginTest ("sanitizer mutes residual when dry and wet at noise floor");
        {
            OutputSanitizer san;
            juce::dsp::ProcessSpec ps { 48000.0, 256, 2 };
            san.prepare (ps);

            juce::AudioBuffer<float> dry (2, 256);
            juce::AudioBuffer<float> wet (2, 256);
            dry.clear();
            for (int i = 0; i < 256; ++i)
            {
                wet.setSample (0, i, 1.0e-6f);
                wet.setSample (1, i, 1.0e-6f);
            }
            for (int b = 0; b < 40; ++b)
            {
                auto dryB = juce::dsp::AudioBlock<const float> (dry);
                auto wetB = juce::dsp::AudioBlock<float> (wet);
                san.process (dryB, wetB);
            }
            float peak = 0.f;
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (wet.getSample (0, i)));
            expect (peak < 1.0e-4f);
        }

        beginTest ("delay process stays finite with high feedback");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 120; feedback = 0.85; mix = 0.5; damp = 3000", err), err);

            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                const float s = 0.5f * std::sin (i * 0.05f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s * 0.8f);
            }
            int bad = 0;
            for (int b = 0; b < 12; ++b)
            {
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
            }
            expectEquals (bad, 0);
        }

        beginTest ("delay formula then dry formula stays finite");
        {
            NeuroCoreAudioProcessor proc;
            proc.prepareToPlay (48000.0, 256);
            juce::String err;
            expect (proc.applyFormula (
                "param a = Time [40, 400]\n"
                "delay1: time = a; feedback = 0.82; mix = 0.65; damp = 3500; pingpong = true\n"
                "filter1: type = lowpass; cutoff = 9000; resonance = 0.3", err), err);

            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            for (int b = 0; b < 16; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = ((i % 32) < 2) ? 0.7f : 0.f;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.8f);
                }
                proc.processBlock (buf, midi);
            }

            expect (proc.applyFormula ("stage1: y = x * 0.5", err), err);
            int bad = 0;
            float peak = 0.f;
            for (int b = 0; b < 16; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.25f * std::sin (i * 0.04f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                proc.processBlock (buf, midi);
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 256; ++i)
                    peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
            }
            expectEquals (bad, 0);
            expect (peak < 4.0f);
            proc.releaseResources();
        }

        // ---- Timeline contracts (architecture, not thresholds) ----
        beginTest ("LatencyAlignedSidechain delay equals configured latency");
        {
            for (int lat : { 0, 1, 26, 64, 128 })
            {
                const int measured = LatencyAlignedSidechain::measureImpulseDelay (lat, 64);
                expectEquals (measured, lat);
            }
        }

        beginTest ("LatencyAlignedSidechain aligns dry with delayed wet timeline");
        {
            // Simulates the crackle root cause: wet delayed by OS FIR, dry not.
            constexpr int L = 26;
            constexpr int N = 128;
            LatencyAlignedSidechain sc;
            sc.prepare (1, N, L);

            juce::AudioBuffer<float> dry (1, N);
            juce::AudioBuffer<float> wetRaw (1, N);   // unaligned dry used as fake wet
            juce::AudioBuffer<float> wetDelayed (1, N);

            // Steady tone
            for (int i = 0; i < N; ++i)
            {
                const float s = 0.5f * std::sin (i * 0.1f);
                dry.setSample (0, i, s);
                wetRaw.setSample (0, i, s);
            }
            // True wet path: delay dry by L samples
            for (int i = 0; i < N; ++i)
                wetDelayed.setSample (0, i, i >= L ? dry.getSample (0, i - L) : 0.f);

            sc.pushAndRead (dry, N);
            const auto& aligned = sc.getAligned();

            // Residual |wetDelayed - aligned| after fill must be ~0
            // Residual |wetDelayed - dry| is large (misaligned fight)
            float errAligned = 0.f, errRaw = 0.f;
            // Second block so ring is filled past latency
            dry.clear();
            for (int i = 0; i < N; ++i)
            {
                const float s = 0.5f * std::sin ((N + i) * 0.1f);
                dry.setSample (0, i, s);
            }
            for (int i = 0; i < N; ++i)
                wetDelayed.setSample (0, i, dry.getSample (0, i)); // will recompute after push
            sc.pushAndRead (dry, N);
            // wet delayed by L relative to current dry:
            // After continuous stream, aligned[i] == dry history at t-L
            // Build delayed wet from continuous phase
            for (int i = 0; i < N; ++i)
            {
                const float delayed = 0.5f * std::sin ((N + i - L) * 0.1f);
                wetDelayed.setSample (0, i, delayed);
                const float a = aligned.getSample (0, i);
                const float r = dry.getSample (0, i);
                errAligned = juce::jmax (errAligned, std::abs (a - delayed));
                errRaw     = juce::jmax (errRaw,     std::abs (r - delayed));
            }
            expect (errAligned < 1.0e-5f);
            expect (errRaw > 0.05f); // proves unaligned comparison is wrong
        }

        beginTest ("filter smoother advances once per sample (stereo)");
        {
            // Architecture: identical L/R input must stay bit-identical.
            // Double-stepping coeff smoothers per channel desyncs stereo timebase.
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "filter1: type = lowpass; cutoff = 1000; resonance = 0.7", err), err);

            juce::AudioBuffer<float> buf (2, 256);
            float phase = 0.f;
            const float dphi = 2.f * juce::MathConstants<float>::pi * 200.f / 48000.f;
            float maxLRDiff = 0.f;
            int bad = 0;
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.5f * std::sin (phase);
                    phase += dphi;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 256; ++i)
                    maxLRDiff = juce::jmax (maxLRDiff,
                        std::abs (buf.getSample (0, i) - buf.getSample (1, i)));
            }
            expectEquals (bad, 0);
            expect (maxLRDiff < 1.0e-5f);
        }
    }
};
