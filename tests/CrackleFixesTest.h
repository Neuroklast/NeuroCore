#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsp/OutputSanitizer.h"
#include "../src/dsp/LatencyAlignedSidechain.h"
#include "../src/utils/ExpressionEvaluator.h"
#include "../src/core/PluginProcessor.h"
#include "../src/core/CpuProtect.h"
#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/utils/FactoryPresetLibrary.h"
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
            NeuroKoreAudioProcessor proc;
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
        beginTest ("mix 0 dry is delayed by reported host latency");
        {
            NeuroKoreAudioProcessor proc;
            proc.setLiveMode (false);
            proc.setPlayConfigDetails (2, 2, 48000.0, 128);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);
            choice->setValueNotifyingHost (choice->convertTo0to1 (2.f)); // 4×
            proc.prepareToPlay (48000.0, 128);
            if (auto* mix = proc.apvts.getParameter (EffectParameters::dryWet))
                mix->setValueNotifyingHost (0.f);

            const int lat = proc.getLatencySamples();
            expect (lat >= 0);
            juce::AudioBuffer<float> buf (2, 128);
            juce::MidiBuffer midi;
            // prepare() starts switchRamp at 0 — eat the fade before the impulse.
            for (int b = 0; b < 48; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
            }
            buf.clear();
            buf.setSample (0, 0, 1.f);
            buf.setSample (1, 0, 1.f);
            proc.processBlock (buf, midi);
            int peakAt = -1;
            float peak = 0.f;
            int sample = 0;
            for (int b = 0; b < 16; ++b)
            {
                if (b > 0)
                {
                    buf.clear();
                    proc.processBlock (buf, midi);
                }
                for (int i = 0; i < 128; ++i, ++sample)
                {
                    const float a = std::abs (buf.getSample (0, i));
                    if (a > peak)
                    {
                        peak = a;
                        peakAt = sample;
                    }
                }
            }
            expect (peak > 0.25f, "dry impulse must appear, peak=" + juce::String (peak, 3));
            if (lat > 0)
                expect (std::abs (peakAt - lat) <= 2,
                        "mix0 peakAt=" + juce::String (peakAt) + " lat=" + juce::String (lat));
            else
                expectEquals (peakAt, 0);
            proc.releaseResources();
        }

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

        beginTest ("CpuProtect trips on consecutive over-budget blocks");
        {
            const double budget = 0.002;
            auto expireWarmup = [] (CpuProtect& g)
            {
                g.observe (0.0, (double) Config::kCpuWarmupSeconds + 1.0);
            };

            // Warmup: many 8× spikes must not trip (cold 8× OS / IR / cache).
            {
                CpuProtect g;
                expect (! g.isTripped());
                const int n = (int) (0.5 * (double) Config::kCpuWarmupSeconds / budget);
                expect (n > 8);
                for (int i = 0; i < n; ++i)
                    g.observe (8.0 * budget, budget);
                expect (! g.isTripped());
                expect (g.getLastLoad() > 7.f);
            }

            // Single 8× spike after warmup must not trip (instant or hard).
            {
                CpuProtect g;
                expireWarmup (g);
                expect (! g.isTripped());
                g.observe (8.0 * budget, budget);
                expect (! g.isTripped());
                expect (g.getLastLoad() > 7.f);
                expect (g.getSmoothedLoad() < g.getLastLoad());
                g.observe (100.0 * budget, budget); // one absurd sample still not a hard trip
                expect (! g.isTripped());
            }

            // Sustained over-budget EMA (2×, below hard) trips after hold time.
            {
                CpuProtect g;
                expireWarmup (g);
                g.observe (0.5 * budget, budget);
                expect (! g.isTripped());
                bool tripped = false;
                const int n = (int) (2.0 * (double) Config::kCpuTripHoldSec / budget) + 8;
                for (int i = 0; i < n; ++i)
                {
                    g.observe (2.0 * budget, budget);
                    if (g.isTripped())
                    {
                        tripped = true;
                        break;
                    }
                }
                expect (tripped);
                expect (g.getSmoothedLoad() >= Config::kCpuTripRatio);

                // After trip: wait retry, cheap probe window recovers.
                expect (! g.shouldProbeWet (64, 48000.0));
                const int need = (int) std::ceil (Config::kCpuRetrySec * 48000.0 / 64.0) + 1;
                bool probed = false;
                for (int i = 0; i < need; ++i)
                    probed = g.shouldProbeWet (64, 48000.0);
                expect (probed);
                const int probeN = (int) (2.0 * (double) Config::kCpuProbeSec / budget) + 4;
                for (int i = 0; i < probeN; ++i)
                    g.observe (0.5 * budget, budget);
                expect (! g.isTripped());
            }

            // A still-hot probe must not untrip — that was the 2 s SAFE blink.
            {
                CpuProtect g;
                expireWarmup (g);
                const int n = (int) (2.0 * (double) Config::kCpuTripHoldSec / budget) + 8;
                for (int i = 0; i < n; ++i)
                    g.observe (2.0 * budget, budget);
                expect (g.isTripped());
                const int need = (int) std::ceil (Config::kCpuRetrySec * 48000.0 / 64.0) + 1;
                for (int i = 0; i < need; ++i)
                    g.shouldProbeWet (64, 48000.0);
                expect (g.shouldProbeWet (64, 48000.0));
                const int probeN = (int) (2.0 * (double) Config::kCpuProbeSec / budget) + 4;
                for (int i = 0; i < probeN; ++i)
                    g.observe (1.8 * budget, budget);
                expect (g.isTripped());
            }

            // Hard trip requires the EMA to stay extreme, not one sample.
            {
                CpuProtect g;
                expireWarmup (g);
                g.observe (10.0 * budget, budget);
                expect (! g.isTripped());

                bool hardTripped = false;
                int hits = 0;
                const int n = (int) (2.0 * (double) Config::kCpuHardHoldSec / budget) + 8;
                for (int i = 0; i < n; ++i)
                {
                    g.observe (10.0 * budget, budget);
                    ++hits;
                    if (g.isTripped())
                    {
                        hardTripped = true;
                        break;
                    }
                }
                expect (hardTripped);
                expect (hits >= Config::kCpuHardTripHits);
                g.noteHoldDisplay();
                expectWithinAbsoluteError (g.getSmoothedLoad(), 0.f, 1.0e-5f);
                expectEquals (Config::cpuDisplayPercent (1.73f), 100);
            }

            {
                CpuProtect g;
                expect (! g.isTripped());
                g.clear();
                expect (! g.isTripped());
            }
        }

        beginTest ("live lock miss does not splice dry input through the output");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 128);
            proc.prepareToPlay (48000.0, 128);
            juce::String err;
            expect (proc.applyFormula ("stage1: y = x * 0.25", err), err);
            juce::AudioBuffer<float> buf (2, 128);
            juce::MidiBuffer midi;
            auto fill = [&] (float amp)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float s = amp * std::sin (0.11f * (float) i);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
            };
            float wetPeak = 0.f;
            for (int b = 0; b < 48; ++b)
            {
                fill (0.8f);
                proc.processBlock (buf, midi);
                if (b >= 40)
                    for (int i = 0; i < 128; ++i)
                        wetPeak = juce::jmax (wetPeak, std::abs (buf.getSample (0, i)));
            }
            expect (wetPeak > 0.12f && wetPeak < 0.35f, "wet peak=" + juce::String (wetPeak, 3));

            float heldPeak = 0.f;
            {
                const juce::ScopedLock sl (proc.getProcessLock());
                fill (0.8f);
                proc.processBlock (buf, midi);
                for (int i = 0; i < 128; ++i)
                    heldPeak = juce::jmax (heldPeak, std::abs (buf.getSample (0, i)));
            }
            expect (heldPeak < 0.40f, "lock-miss must not emit dry 0.8, peak="
                    + juce::String (heldPeak, 3));
            expect (heldPeak > 0.05f, "lock-miss should keep last wet, peak="
                    + juce::String (heldPeak, 3));
            proc.releaseResources();
        }

        beginTest ("8x then 4x oversampling does not process leftover silence");
        {
            // Grow-only scriptBuffer used to keep the 8× length after dropping to 4×.
            // DSL then ran an extra half-block of zeros every callback → periodic glitch.
            NeuroKoreAudioProcessor proc;
            proc.prepareToPlay (48000.0, 64);
            juce::String err;
            expect (proc.applyFormula ("stage1: y = x", err), err);

            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            auto fillSine = [&] (int phase0)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float s = 0.5f * std::sin ((phase0 + i) * 0.11f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
            };

            choice->setValueNotifyingHost (choice->convertTo0to1 (3.f)); // 8×
            proc.prepareToPlay (48000.0, 64);
            for (int b = 0; b < 12; ++b)
            {
                fillSine (b * 64);
                proc.processBlock (buf, midi);
            }

            choice->setValueNotifyingHost (choice->convertTo0to1 (2.f)); // 4×
            proc.prepareToPlay (48000.0, 64);
            float maxJump = 0.f;
            float last = 0.f;
            bool have = false;
            int bad = 0;
            for (int b = 0; b < 24; ++b)
            {
                fillSine (12 * 64 + b * 64);
                proc.processBlock (buf, midi);
                bad += TestHelpers::countNonFinite (buf);
                // Skip the first few blocks (switch ramp / OS FIR fill)
                if (b < 6)
                    continue;
                for (int i = 0; i < 64; ++i)
                {
                    const float y = buf.getSample (0, i);
                    if (have)
                        maxJump = juce::jmax (maxJump, std::abs (y - last));
                    last = y;
                    have = true;
                }
            }
            expectEquals (bad, 0);
            // A 0.5 sine at this step has |Δ| << 0.2; leftover-zero blocks spike near 0.5
            expect (maxJump < 0.22f);
            proc.releaseResources();
        }

        beginTest ("switch up to 8x stays finite");
        {
            NeuroKoreAudioProcessor proc;
            proc.prepareToPlay (48000.0, 64);
            juce::String err;
            expect (proc.applyFormula ("stage1: y = softclip(x, 2.2)", err), err);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);
            choice->setValueNotifyingHost (choice->convertTo0to1 (1.f));
            proc.prepareToPlay (48000.0, 64);
            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            for (int b = 0; b < 4; ++b)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float s = 0.4f * std::sin (i * 0.2f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                proc.processBlock (buf, midi);
            }
            choice->setValueNotifyingHost (choice->convertTo0to1 (3.f));
            proc.prepareToPlay (48000.0, 64);
            int bad = 0;
            for (int b = 0; b < 16; ++b)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float s = 0.4f * std::sin ((b * 64 + i) * 0.2f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                proc.processBlock (buf, midi);
                bad += TestHelpers::countNonFinite (buf);
            }
            expectEquals (bad, 0);
            expect (proc.getLatencySamples() > 0);
            proc.releaseResources();
        }

        beginTest ("oversampling change stays finite (no sticky crackle)");
        {
            NeuroKoreAudioProcessor proc;
            proc.prepareToPlay (48000.0, 128);
            juce::String err;
            expect (proc.applyFormula (
                "env1: type = peak; attack = 0.002; release = 0.16\n"
                "filter1: type = lowpass; cutoff = 180; + = env1; * = 2400; resonance = 1.8\n"
                "stage1: y = diode(x, 2.0)", err), err);

            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);

            juce::AudioBuffer<float> buf (2, 128);
            juce::MidiBuffer midi;
            int bad = 0;
            int lastLat = -1;
            for (int idx : { 1, 2, 3, 0, 1 })
            {
                choice->setValueNotifyingHost (choice->convertTo0to1 ((float) idx));
                // AsyncUpdater is a private base — re-prepare reads the new choice index
                // the same way handleAsyncUpdate / prepareToPlay does in the host.
                proc.prepareToPlay (48000.0, 128);
                const int lat = proc.getLatencySamples();
                expect (lat >= Config::kSanitationLookaheadHost);
                if (idx == 0)
                    expectEquals (lat, Config::kSanitationLookaheadHost);
                else
                    expect (lat > Config::kSanitationLookaheadHost);
                lastLat = lat;
                for (int b = 0; b < 8; ++b)
                {
                    for (int i = 0; i < 128; ++i)
                    {
                        const float s = 0.4f * std::sin (i * 0.07f);
                        buf.setSample (0, i, s);
                        buf.setSample (1, i, s);
                    }
                    proc.processBlock (buf, midi);
                    bad += TestHelpers::countNonFinite (buf);
                }
            }
            expectEquals (bad, 0);
            expect (lastLat >= 0);
            proc.releaseResources();
        }

        beginTest ("loudness meter falls on silence when mix is dry");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 128);
            proc.prepareToPlay (48000.0, 128);
            if (auto* mix = proc.apvts.getParameter (EffectParameters::dryWet))
                mix->setValueNotifyingHost (0.f);

            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buf (2, 128);
            for (int b = 0; b < 80; ++b)
            {
                for (int i = 0; i < 128; ++i)
                {
                    buf.setSample (0, i, 0.8f);
                    buf.setSample (1, i, 0.8f);
                }
                proc.processBlock (buf, midi);
            }
            const float loud = proc.getLoudnessDb();
            expect (loud > -12.f);

            for (int b = 0; b < 80; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
            }
            expect (proc.getLoudnessDb() < loud - 12.f);
            proc.releaseResources();
        }

        beginTest ("8x delay impulse has no periodic wrap ticks");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 64);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);
            choice->setValueNotifyingHost (choice->convertTo0to1 (3.f)); // 8×
            proc.prepareToPlay (48000.0, 64);
            juce::String err;
            expect (proc.applyFormula (
                "delay1: time = 80; feedback = 0.0; mix = 1.0; damp = 12000", err), err);

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            buf.clear();
            buf.setSample (0, 0, 0.9f);
            buf.setSample (1, 0, 0.9f);
            proc.processBlock (buf, midi);

            const int echoHost = (int) std::lround (0.080 * 48000.0); // 3840
            float laterPeak = 0.f;
            int extraPeaks = 0;
            int sample = 64;
            for (int b = 0; b < 200; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
                for (int i = 0; i < 64; ++i, ++sample)
                {
                    const float a = std::abs (buf.getSample (0, i));
                    const bool nearEcho = std::abs (sample - echoHost) < 96;
                    if (nearEcho)
                        continue;
                    laterPeak = juce::jmax (laterPeak, a);
                    if (a > 0.07f)
                        ++extraPeaks;
                }
            }
            expect (laterPeak < 0.07f, "8x delay wrap/block tick peak="
                    + juce::String (laterPeak, 4) + " extras=" + juce::String (extraPeaks));
            proc.releaseResources();
        }

        beginTest ("Tape Echo Dirt at 8x has no periodic ticks");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            const auto* preset = lib.findByName ("Tape Echo Dirt");
            expect (preset != nullptr);
            if (preset == nullptr)
                return;

            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 64);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);
            choice->setValueNotifyingHost (choice->convertTo0to1 (3.f));
            proc.prepareToPlay (48000.0, 64);
            juce::String err;
            expect (lib.applyPreset (proc, (int) (preset - lib.getEntries().data()), err), err);

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            buf.clear();
            buf.setSample (0, 0, 0.85f);
            buf.setSample (1, 0, 0.85f);
            proc.processBlock (buf, midi);

            // Default Time ≈ 320 ms. Allow the first echo and its decay; reject
            // ticks that repeat at the host block (64) or a too-short wrap.
            const int echoHost = (int) std::lround (0.320 * 48000.0);
            std::vector<int> peakAt;
            int sample = 64;
            for (int b = 0; b < 400; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
                for (int i = 0; i < 64; ++i, ++sample)
                {
                    if (std::abs (buf.getSample (0, i)) > 0.08f)
                        peakAt.push_back (sample);
                }
            }

            int odd = 0;
            for (int p : peakAt)
            {
                bool nearEcho = false;
                for (int k = 1; k <= 6; ++k)
                    if (std::abs (p - k * echoHost) < 200)
                        nearEcho = true;
                if (! nearEcho)
                    ++odd;
            }
            expect (odd < 8, "Tape Echo Dirt 8x extra ticks=" + juce::String (odd)
                    + " of " + juce::String ((int) peakAt.size()));
            proc.releaseResources();
        }

        beginTest ("8x Tape Echo Dirt sine has no block-rate clicks");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            int idx = -1;
            for (int i = 0; i < (int) lib.getEntries().size(); ++i)
                if (lib.getEntries()[(size_t) i].name == "Tape Echo Dirt")
                    idx = i;
            expect (idx >= 0);
            if (idx < 0)
                return;

            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 64);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);
            choice->setValueNotifyingHost (choice->convertTo0to1 (3.f));
            proc.prepareToPlay (48000.0, 64);
            juce::String err;
            expect (lib.applyPreset (proc, idx, err), err);

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            float prev = 0.f;
            bool have = false;
            float maxInterior = 0.f, maxBoundary = 0.f;
            for (int b = 0; b < 120; ++b)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float t = (float) (b * 64 + i) / 48000.f;
                    const float s = 0.35f * std::sin (2.f * juce::MathConstants<float>::pi * 220.f * t);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                proc.processBlock (buf, midi);
                if (b < 24)
                {
                    prev = buf.getSample (0, 63);
                    have = true;
                    continue;
                }
                for (int i = 0; i < 64; ++i)
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
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (maxBoundary < maxInterior * 2.5f + 0.02f,
                    "8x delay block click: boundary=" + juce::String (maxBoundary, 4)
                    + " interior=" + juce::String (maxInterior, 4));
            proc.releaseResources();
        }

        beginTest ("heavy tape/delay factory presets stay finite at 8x");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));

            const juce::String names[] = {
                "Tape Echo Heads",
                "Analog Bucket Echo",
                "Tape Slap Echo",
                "Tape Echo Dirt",
                "Digital Grid Delay",
                "Glitch Laboratory",
                "Phaser Lab",
                "Dotted Eighth Throw",
                "Cascade Taps",
                "Cinematic Space",
                "Far Plane",
                "Kick Rumble",
                "Warehouse Rumble",
                "Hardcore Clip",
                "Gabber Drive",
            };

            for (const auto& name : names)
            {
                int idx = -1;
                for (int i = 0; i < (int) lib.getEntries().size(); ++i)
                    if (lib.getEntries()[(size_t) i].name == name)
                        idx = i;
                expect (idx >= 0, "missing factory " + name);
                if (idx < 0)
                    continue;

                NeuroKoreAudioProcessor proc;
                proc.setPlayConfigDetails (2, 2, 48000.0, 64);
                auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    proc.apvts.getParameter (EffectParameters::oversampling));
                expect (choice != nullptr);
                choice->setValueNotifyingHost (choice->convertTo0to1 (3.f));
                proc.prepareToPlay (48000.0, 64);
                juce::String err;
                expect (lib.applyPreset (proc, idx, err), name + " " + err);

                juce::AudioBuffer<float> buf (2, 64);
                juce::MidiBuffer midi;
                float prev = 0.f;
                bool have = false;
                float maxBoundary = 0.f, maxInterior = 0.f;
                int bad = 0;
                for (int b = 0; b < 90; ++b)
                {
                    for (int i = 0; i < 64; ++i)
                    {
                        const float t = (float) (b * 64 + i) / 48000.f;
                        const float s = 0.32f * std::sin (2.f * juce::MathConstants<float>::pi * 220.f * t);
                        buf.setSample (0, i, s);
                        buf.setSample (1, i, s);
                    }
                    proc.processBlock (buf, midi);
                    bad += TestHelpers::countNonFinite (buf);
                    if (b < 20)
                    {
                        prev = buf.getSample (0, 63);
                        have = true;
                        continue;
                    }
                    for (int i = 0; i < 64; ++i)
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
                expectEquals (bad, 0, name + " produced NaN/Inf");
                expect (maxBoundary < maxInterior * 2.8f + 0.025f,
                        name + " 8x block click: boundary=" + juce::String (maxBoundary, 4)
                        + " interior=" + juce::String (maxInterior, 4));
                expect (! proc.isCpuProtectActive(), name + " tripped CPU protect");
                proc.releaseResources();
            }
        }

        beginTest ("env peak of a hot signal stays in 0..1 (no gain invert)");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "env1: type = peak; attack = 0.001; release = 0.05\n"
                "stage1: y = x * (1.0 - env1 * 0.85)", err), err);

            juce::AudioBuffer<float> buf (2, 256);
            float minY = 1.0e9f, maxY = -1.0e9f, maxJump = 0.f, prev = 0.f;
            bool have = false;
            for (int b = 0; b < 12; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float t = (float) (b * 256 + i) / 48000.f;
                    const float s = 1.8f * std::sin (2.f * juce::MathConstants<float>::pi * 120.f * t);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                for (int i = 0; i < 256; ++i)
                {
                    const float y = buf.getSample (0, i);
                    expect (std::isfinite (y));
                    minY = juce::jmin (minY, y);
                    maxY = juce::jmax (maxY, y);
                    if (have)
                        maxJump = juce::jmax (maxJump, std::abs (y - prev));
                    prev = y;
                    have = true;
                }
            }
            expect (maxJump < 0.35f, "env duck jump " + juce::String (maxJump, 4));
            expect (minY > -2.2f && maxY < 2.2f);
        }

        beginTest ("Acid Line env-open filter stays finite without a hitch");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "param a = Res [0.8, 3.4]\n"
                "param b = Min [90, 500]\n"
                "param c = Range [500, 4500]\n"
                "param d = Drive [1.1, 4.0]\n"
                "env1: type = peak; attack = 0.002; release = 0.16\n"
                "filter1: type = lowpass; cutoff = b; + = env1; * = c; resonance = a\n"
                "stage1: y = diode(x, d)\n", err), err);
            juce::AudioBuffer<float> buf (2, 256);
            float maxJump = 0.f, prev = 0.f;
            bool have = false;
            for (int b = 0; b < 40; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float t = (float) (b * 256 + i) / 48000.f;
                    const float s = ((b % 8) < 2 ? 0.85f : 0.02f)
                                  * std::sin (2.f * juce::MathConstants<float>::pi * 110.f * t);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                for (int i = 0; i < 256; ++i)
                {
                    const float y = buf.getSample (0, i);
                    expect (std::isfinite (y));
                    if (have)
                        maxJump = juce::jmax (maxJump, std::abs (y - prev));
                    prev = y;
                    have = true;
                }
            }
            expect (maxJump < 1.8f, "acid line jump " + juce::String (maxJump, 4));
        }

        beginTest ("preset switch Acid Line to Metal Gate stays finite and env resets");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "env1: type = peak; attack = 0.004; release = 0.16\n"
                "filter1: type = lowpass; cutoff = 180; + = env1; * = 2400; resonance = 1.9\n"
                "stage1: y = diode(x, 2)\n", err), err);
            juce::AudioBuffer<float> buf (2, 128);
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float s = 0.7f * std::sin (0.2f * (float) (b * 128 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            }
            chain.clearRuntimeState();
            expect (chain.loadScript (
                "gate1: threshold = -38; hyst = 8; hold = 0.08; release = 0.12; range = -80\n"
                "stage1: y = tube(x, 4)\n"
                "widen1: width = 0.7\n", err), err);
            chain.clearRuntimeState();
            float maxAbs = 0.f;
            for (int b = 0; b < 12; ++b)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float s = 0.4f * std::sin (0.11f * (float) (b * 128 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                for (int i = 0; i < 128; ++i)
                {
                    const float l = buf.getSample (0, i);
                    const float r = buf.getSample (1, i);
                    expect (std::isfinite (l) && std::isfinite (r));
                    maxAbs = juce::jmax (maxAbs, std::abs (l), std::abs (r));
                }
            }
            expect (maxAbs > 0.02f);
        }

        beginTest ("widen after a mono IR still opens L/R");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "stage1: y = x\n"
                "ir1: mix = 0.5; gain = 0\n"
                "widen1: width = 1.0\n", err), err);
            juce::AudioBuffer<float> ir (1, 64);
            ir.clear();
            ir.setSample (0, 0, 1.f);
            chain.loadImpulseResponse ("ir1", ir, 48000.0);
            juce::AudioBuffer<float> buf (2, 256);
            float side = 0.f;
            for (int b = 0; b < 20; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.5f * std::sin (2.f * juce::MathConstants<float>::pi
                                                     * 440.f * (float) (b * 256 + i) / 48000.f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                for (int i = 0; i < 256; ++i)
                    side = juce::jmax (side, std::abs (buf.getSample (0, i) - buf.getSample (1, i)));
            }
            expect (side > 0.02f, "widen side=" + juce::String (side, 4));
        }

        beginTest ("phaser sweep does not slam stereo width each LFO cycle");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "param a = Rate [0.08, 3.0]\n"
                "param b = Center [300, 1600]\n"
                "param c = Depth [200, 2200]\n"
                "param d = Mix [0.25, 0.9]\n"
                "osc1: shape = sine; freq = 2.2; depth = 1.0\n"
                "bus wet:\n"
                "  send: in = 1\n"
                "  filter1: type = highpass; cutoff = b + (0.5 + 0.5 * osc1) * (c * 0.22); resonance = 0.55\n"
                "  filter2: type = lowpass; cutoff = b + c * 0.7 + (0.5 + 0.5 * osc1) * (c * 0.28); resonance = 0.45\n"
                "out: main = 1-d; wet = d\n", err), err);

            juce::AudioBuffer<float> buf (2, 128);
            float maxJump = 0.f, prev = 0.f;
            bool have = false;
            int bad = 0;
            for (int b = 0; b < 40; ++b)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float t = (float) (b * 128 + i) / 48000.f;
                    const float s = 0.35f * std::sin (2.f * juce::MathConstants<float>::pi * 440.f * t);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.7f);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 128; ++i)
                {
                    const float y = buf.getSample (0, i);
                    if (have)
                        maxJump = juce::jmax (maxJump, std::abs (y - prev));
                    prev = y;
                    have = true;
                }
            }
            expectEquals (bad, 0);
            expect (maxJump < 0.28f, "phaser stereo slam jump=" + juce::String (maxJump, 4));
        }

        beginTest ("octaver on a kick impulse does not click each period");
        {
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "octaver1: sub = 1.0; up = 0; mix = 0.7; tone = 120; thresh = 0.03",
                err), err);

            juce::AudioBuffer<float> buf (2, 256);
            float maxJump = 0.f, prev = 0.f;
            bool have = false;
            int bad = 0;
            for (int b = 0; b < 24; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float t = (float) (b * 256 + i) / 48000.f;
                    const float body = std::exp (-t / 0.08f)
                        * std::sin (2.f * juce::MathConstants<float>::pi * (55.f - 16.f * t) * t);
                    const float click = (t < 0.004f)
                        ? std::exp (-t / 0.0012f)
                            * std::sin (2.f * juce::MathConstants<float>::pi * 2200.f * t)
                        : 0.f;
                    const float s = 0.85f * body + 0.35f * click;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 256; ++i)
                {
                    const int absI = b * 256 + i;
                    const float y = buf.getSample (0, i);
                    // Skip the dry kick attack; we care about octaver period flips.
                    if (have && absI > 480)
                        maxJump = juce::jmax (maxJump, std::abs (y - prev));
                    prev = y;
                    have = true;
                }
            }
            expectEquals (bad, 0);
            expect (maxJump < 0.22f, "octaver kick jump=" + juce::String (maxJump, 4));
        }

        beginTest ("Kick Rumble kick impulse has no delayed click");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            int idx = -1;
            for (int i = 0; i < (int) lib.getEntries().size(); ++i)
                if (lib.getEntries()[(size_t) i].name == "Kick Rumble")
                    idx = i;
            expect (idx >= 0, "missing factory Kick Rumble");
            if (idx < 0)
                return;

            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 64);
            proc.prepareToPlay (48000.0, 64);
            juce::String err;
            expect (lib.applyPreset (proc, idx, err), err);

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            float maxLate = 0.f, prev = 0.f;
            bool have = false;
            int bad = 0;
            float peakTail = 0.f;
            for (int b = 0; b < 80; ++b)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float t = (float) (b * 64 + i) / 48000.f;
                    const float body = std::exp (-t / 0.075f)
                        * std::sin (2.f * juce::MathConstants<float>::pi * (50.f - 14.f * t) * t);
                    const float click = (t < 0.004f)
                        ? std::exp (-t / 0.0011f)
                            * std::sin (2.f * juce::MathConstants<float>::pi * 2400.f * t)
                        : 0.f;
                    const float s = 0.82f * body + 0.4f * click;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                proc.processBlock (buf, midi);
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 64; ++i)
                {
                    const int absI = b * 64 + i;
                    const float y = buf.getSample (0, i);
                    if (absI > 480)
                        peakTail = juce::jmax (peakTail, std::abs (y));
                    // After the dry attack, no second click at the 15.5 ms tap.
                    if (have && absI > 480)
                        maxLate = juce::jmax (maxLate, std::abs (y - prev));
                    prev = y;
                    have = true;
                }
            }
            expectEquals (bad, 0, "Kick Rumble produced NaN/Inf");
            expect (peakTail > 0.02f, "Kick Rumble went silent");
            expect (maxLate < 0.28f, "Kick Rumble late jump=" + juce::String (maxLate, 4));
            expect (! proc.isCpuProtectActive(), "Kick Rumble tripped CPU protect");
            proc.releaseResources();
        }

        beginTest ("Far Plane impulse has no slap at predelay");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            const auto* entry = lib.findByName ("Far Plane");
            expect (entry != nullptr, "missing factory Far Plane");
            if (entry == nullptr)
                return;

            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (entry->script, err), err);

            juce::AudioBuffer<float> buf (2, 256);
            float maxSlap = 0.f, firstPeak = 0.f;
            int bad = 0;
            for (int b = 0; b < 24; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const int absI = b * 256 + i;
                    const float s = (absI == 8) ? 0.85f : 0.f;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
                for (int i = 0; i < 256; ++i)
                {
                    const int absI = b * 256 + i;
                    const float y = std::abs (buf.getSample (0, i));
                    if (absI < 512)
                        firstPeak = juce::jmax (firstPeak, y);
                    // Default Predelay = 95 ms → sample ~4560. A series slap
                    // (old mix=0.38) reprints the impulse there.
                    if (absI >= 4300 && absI < 4900)
                        maxSlap = juce::jmax (maxSlap, y);
                }
            }
            expectEquals (bad, 0, "Far Plane produced NaN/Inf");
            expect (firstPeak > 0.05f, "Far Plane dry went silent");
            expect (maxSlap < firstPeak * 0.55f,
                    "Far Plane slap at ~95 ms: slap=" + juce::String (maxSlap, 4)
                    + " first=" + juce::String (firstPeak, 4));
        }

        beginTest ("silence after tail and hold sleeps wet OS/DSL");
        {
            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 128);
            proc.prepareToPlay (48000.0, 128);
            juce::String err;
            expect (proc.applyFormula ("stage1: y = x * 0.8", err), err);
            if (auto* mix = proc.apvts.getParameter (EffectParameters::dryWet))
                mix->setValueNotifyingHost (1.f);

            juce::AudioBuffer<float> buf (2, 128);
            juce::MidiBuffer midi;
            // switchRamp (80 ms) + idle hold (120 ms) + OS flush. 0.53 s is enough.
            for (int b = 0; b < 200; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
            }
            expect (proc.isDspIdle(), "wet OS/DSL must sleep after silence + tail flush");

            for (int i = 0; i < 128; ++i)
            {
                buf.setSample (0, i, 0.4f);
                buf.setSample (1, i, 0.4f);
            }
            proc.processBlock (buf, midi);
            expect (! proc.isDspIdle(), "a transient must wake the wet path on the same block");
            proc.releaseResources();
        }
    }
};
