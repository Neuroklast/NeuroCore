#pragma once

#include <JuceHeader.h>
#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/dsp/DSPUtils.h"
#include "../src/dsp/OutputSanitizer.h"
#include "../src/dsp/LatencyAlignedSidechain.h"
#include "../src/dsl/SignalChain.h"
#include "TestHelpers.h"
#include <cmath>
#include <vector>

/** TDD contracts for non-UI architecture hardening (A1–A10). */
class ArchitectureHardeningTest : public juce::UnitTest
{
public:
    ArchitectureHardeningTest() : juce::UnitTest ("ArchitectureHardening") {}

    void runTest() override
    {
        // ---- A1: diagnostics default off ----
        beginTest ("diagnostics disabled by default");
        {
            expect (! Config::kAudioDiagnosticsEnabled);
        }

        // ---- A2: autoGain strength ----
        beginTest ("autoGain strength 0 leaves wet unchanged");
        {
            juce::AudioBuffer<float> dry (1, 8), wet (1, 8);
            for (int i = 0; i < 8; ++i)
            {
                dry.setSample (0, i, 1.0f);
                wet.setSample (0, i, 0.5f);
            }
            juce::SmoothedValue<float> sm;
            sm.reset (44100.0, 0.0);
            sm.setCurrentAndTargetValue (1.0f);
            juce::dsp::Gain<float> gain;
            juce::dsp::ProcessSpec spec { 44100.0, 8, 1 };
            gain.prepare (spec);
            juce::dsp::AudioBlock<float> dB (dry), wB (wet);
            DSPUtils::autoGainCompensate (dB, wB, sm, gain, 0.0f);
            for (int i = 0; i < 8; ++i)
                expectWithinAbsoluteError (wet.getSample (0, i), 0.5f, 1.0e-5f);
        }

        beginTest ("autoGain strength 1 applies partial makeup");
        {
            juce::AudioBuffer<float> dry (2, 4), wet (2, 4);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4; ++i)
                {
                    dry.setSample (ch, i, 1.0f);
                    wet.setSample (ch, i, 0.5f);
                }
            juce::SmoothedValue<float> sm;
            sm.reset (44100.0, 0.0);
            sm.setCurrentAndTargetValue (1.0f);
            juce::dsp::Gain<float> gain;
            juce::dsp::ProcessSpec spec { 44100.0, 4, 2 };
            gain.prepare (spec);
            juce::dsp::AudioBlock<float> dB (dry), wB (wet);
            DSPUtils::autoGainCompensate (dB, wB, sm, gain, 1.0f);
            // full=2, blend=0.25 → corr=1.25 → 0.5*1.25=0.625
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 4; ++i)
                    expectWithinAbsoluteError (wet.getSample (ch, i), 0.625f, 1.0e-4f);
        }

        // ---- A3: OS default index is 2x (choice index 1) ----
        beginTest ("oversampling default choice index is 2x (1)");
        {
            // Contract: default stage index must be 1 (= 2×), not 2 (= 4×)
            expectEquals (Config::kDefaultOversamplingIndex, 1);
        }

        // ---- A5: single peak boundary ----
        beginTest ("sanitizer applies soft peak when peak safety enabled");
        {
            OutputSanitizer san;
            san.prepare ({ 48000.0, 64, 1 });
            san.setPeakSafetyEnabled (true);
            juce::AudioBuffer<float> dry (1, 64), wet (1, 64);
            dry.clear();
            for (int i = 0; i < 64; ++i)
                wet.setSample (0, i, 2.0f);
            for (int b = 0; b < 8; ++b)
            {
                auto d = juce::dsp::AudioBlock<const float> (dry);
                auto w = juce::dsp::AudioBlock<float> (wet);
                san.process (d, w);
            }
            float peak = 0.f;
            for (int i = 0; i < 64; ++i)
                peak = juce::jmax (peak, std::abs (wet.getSample (0, i)));
            expect (peak < 1.05f);
        }

        beginTest ("sanitizer skips peak stage when peak owned upstream");
        {
            OutputSanitizer san;
            san.prepare ({ 48000.0, 64, 1 });
            san.setPeakSafetyEnabled (false);
            juce::AudioBuffer<float> dry (1, 64), wet (1, 64);
            // dry must have energy so residual gate stays open
            for (int i = 0; i < 64; ++i)
            {
                dry.setSample (0, i, 0.5f);
                wet.setSample (0, i, 2.0f);
            }
            for (int b = 0; b < 4; ++b)
            {
                auto d = juce::dsp::AudioBlock<const float> (dry);
                auto w = juce::dsp::AudioBlock<float> (wet);
                san.process (d, w);
            }
            float peak = 0.f;
            for (int i = 0; i < 64; ++i)
                peak = juce::jmax (peak, std::abs (wet.getSample (0, i)));
            // No peak soft-ceil → still near 2.0
            expect (peak > 1.5f);
        }

        // ---- A7: silence leak only on feedback stages ----
        beginTest ("feedback silence leak constant is named in Config");
        {
            expect (Config::kFeedbackSilenceLeak > 0.0f);
            expect (Config::kFeedbackSilenceLeak < 1.0f);
        }

        beginTest ("non-feedback stage does not force silence leak decay on x_prev path");
        {
            juce::dsp::ProcessSpec spec { 48000.0, 256, 1 };
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript ("stage1: y = x", err), err);

            juce::AudioBuffer<float> buf (1, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 0.5f);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());

            // silence — non-feedback should pass zeros, not self-oscillate
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 0.0f);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            float peak = 0.f;
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
            expect (peak < 1.0e-5f);
        }

        // ---- A6: delay feedback architecture ----
        beginTest ("delay feedback 0.95 stays finite and decays after impulse");
        {
            juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "delay1: time = 100; feedback = 0.95; mix = 1.0; damp = 8000", err), err);

            juce::AudioBuffer<float> buf (2, 256);
            buf.clear();
            buf.setSample (0, 0, 0.8f);
            buf.setSample (1, 0, 0.8f);
            float earlyPeak = 0.f, latePeak = 0.f;
            int bad = 0;
            // ~0.4 s @ 48k — enough to prove decay without 100k expects
            for (int b = 0; b < 80; ++b)
            {
                if (b > 0)
                    buf.clear();
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                bad += TestHelpers::countNonFinite (buf);
                const float pk = TestHelpers::peakAbs (buf);
                if (b < 10)
                    earlyPeak = juce::jmax (earlyPeak, pk);
                if (b >= 70)
                    latePeak = juce::jmax (latePeak, pk);
            }
            expectEquals (bad, 0);
            expect (earlyPeak > 0.01f);
            expect (latePeak < earlyPeak * 0.5f);
        }

        beginTest ("higher delay feedback yields longer tail than lower");
        {
            // 50 ms delay @ 48 kHz ≈ 2400 samples. First echo ~block 19; second ~block 38.
            // Compare energy only after the 2nd recirculation so feedback gain dominates.
            auto tailEnergy = [] (float fb) -> float
            {
                juce::dsp::ProcessSpec spec { 48000.0, 128, 1 };
                dsl::SignalChain chain;
                chain.prepare (spec);
                juce::String err;
                juce::String script = "delay1: time = 50; feedback = "
                    + juce::String (fb, 3) + "; mix = 1.0; damp = 18000";
                if (! chain.loadScript (script, err))
                    return -1.f;
                juce::AudioBuffer<float> buf (1, 128);
                buf.clear();
                buf.setSample (0, 0, 1.0f);
                double acc = 0.0;
                for (int b = 0; b < 70; ++b)
                {
                    if (b > 0)
                        buf.clear();
                    chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                    if (b >= 40)
                        for (int i = 0; i < 128; ++i)
                        {
                            const float v = buf.getSample (0, i);
                            acc += (double) v * (double) v;
                        }
                }
                return (float) acc;
            };
            const float eLow  = tailEnergy (0.4f);
            const float eHigh = tailEnergy (0.92f);
            expect (eLow >= 0.f && eHigh >= 0.f);
            expect (eHigh > eLow * 1.5f,
                    "high FB energy=" + juce::String (eHigh, 6)
                        + " low FB energy=" + juce::String (eLow, 6));
        }

        // ---- A8: sidechain block path preserves latency ----
        beginTest ("LatencyAlignedSidechain impulse delay contract still holds");
        {
            for (int lat : { 0, 1, 26, 64 })
                expectEquals (LatencyAlignedSidechain::measureImpulseDelay (lat, 64), lat);
        }

        // ---- A9: continuous wet mix helper ----
        beginTest ("continuous wet mix interpolates across block");
        {
            juce::AudioBuffer<float> dry (1, 8), wet (1, 8), out (1, 8);
            for (int i = 0; i < 8; ++i)
            {
                dry.setSample (0, i, 0.0f);
                wet.setSample (0, i, 1.0f);
            }
            DSPUtils::mixDryWetContinuous (dry, wet, out, 0.0f, 1.0f);
            // First sample near dry (0), last near wet (1)
            expect (out.getSample (0, 0) < 0.2f);
            expect (out.getSample (0, 7) > 0.8f);
            // Monotonic increase
            for (int i = 1; i < 8; ++i)
                expect (out.getSample (0, i) >= out.getSample (0, i - 1) - 1.0e-6f);
        }

        // ---- Mix 0% must be pure dry (no wet DSL in output) ----
        beginTest ("mixDryWetContinuous at 0 keeps pure dry");
        {
            juce::AudioBuffer<float> dry (1, 16), wet (1, 16), out (1, 16);
            for (int i = 0; i < 16; ++i)
            {
                dry.setSample (0, i, 0.25f);
                wet.setSample (0, i, 0.9f); // would crackle if leaked
            }
            DSPUtils::mixDryWetContinuous (dry, wet, out, 0.0f, 0.0f);
            for (int i = 0; i < 16; ++i)
                expectWithinAbsoluteError (out.getSample (0, i), 0.25f, 1.0e-6f);
        }

        // ---- More than 4 knobs ----
        beginTest ("engine supports 6 user params a-f");
        {
            expectEquals (Config::kNumUserParams, 6);
            expect (juce::String (Config::kDefaultVariableNames[0]) == "a");
            expect (juce::String (Config::kDefaultVariableNames[5]) == "f");
        }

        beginTest ("signal chain routes param e-f");
        {
            juce::dsp::ProcessSpec spec { 48000.0, 64, 1 };
            dsl::SignalChain chain;
            chain.prepare (spec);
            juce::String err;
            expect (chain.loadScript (
                "param e = Extra [0, 1]\nparam f = Hi [0, 1]\nstage1: y = x * e * (0.5 + 0.5 * f)", err), err);

            juce::SmoothedValue<float> sm[Config::kNumUserParams];
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                sm[p].reset (48000.0, 0.0);
                // e=0.5, f=1 -> y = 1 * 0.5 * (0.5+0.5) = 0.5
                float v = 0.f;
                if (p == 4) v = 0.5f;
                if (p == 5) v = 1.0f;
                sm[p].setCurrentAndTargetValue (v);
            }
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> ptrs {};
            for (int p = 0; p < Config::kNumUserParams; ++p)
                ptrs[(size_t) p] = &sm[p];

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlockSmoothed (buf, ptrs);
            float peak = 0.f;
            for (int i = 0; i < 64; ++i)
                peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
            expectWithinAbsoluteError (peak, 0.5f, 0.05f);
        }
    }
};
