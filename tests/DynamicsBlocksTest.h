#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "../src/core/Config.h"
#include "../src/core/MidiVariableMapper.h"
#include "TestHelpers.h"
#include <array>
#include <cmath>

class DynamicsBlocksTest : public juce::UnitTest
{
public:
    DynamicsBlocksTest() : juce::UnitTest ("DynamicsBlocksTest", "DSP") {}

    void runTest() override
    {
        beginTest ("noisegate parses with optional attack release threshold");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse ("ngate1: threshold = -50", blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 1);
            expectEquals (blocks[0].type, juce::String ("noisegate"));
            dsl::SignalChain chain;
            expect (chain.loadScript ("ngate1: threshold = -24; attack = 0.001; release = 0.02", err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            const float quiet = tonePeak (chain, 0.01f, 200.f, 48000.f, 8);
            const float loud  = tonePeak (chain, 0.4f, 200.f, 48000.f, 8);
            expect (quiet < 0.03f, "quiet tone should be gated, peak=" + juce::String (quiet, 4));
            expect (loud  > 0.15f, "loud tone should pass, peak=" + juce::String (loud, 4));
        }

        beginTest ("process writes a node tap the canvas can copy");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("stage1: y = x", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                const float s = 0.4f * std::sin (2.f * juce::MathConstants<float>::pi * 200.f * (float) i / 48000.f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            chain.processBlock (buf);
            float dest[64];
            expect (chain.copyNodeTap ("stage1", dest, 64), "stage tap missing");
            expect (chain.copyNodeTap ("__out__", dest, 64), "out tap missing");
            expect (chain.copyNodeTap ("__in__", dest, 64), "in tap missing");
            float peak = 0.f;
            for (float s : dest)
                peak = juce::jmax (peak, std::abs (s));
            expect (peak > 0.05f, "tap wave should carry the tone");
        }

        beginTest ("gate block parses");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse (
                "gate1: threshold = -42; hyst = 3; attack = 0.001; hold = 0.04; release = 0.08; range = -70",
                blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 1);
            expect (blocks[0].type.startsWith ("gate"));
        }

        beginTest ("gate closes on a quiet sine and opens on a loud one");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "gate1: threshold = -24; hyst = 3; attack = 0.001; hold = 0.01; release = 0.02; range = -80",
                err), err);
            chain.prepare ({ 48000.0, 512, 2 });

            const float quiet = tonePeak (chain, 0.01f, 200.f, 48000.f, 8);
            const float loud  = tonePeak (chain, 0.35f, 200.f, 48000.f, 8);
            expect (quiet < 0.02f, "quiet tone should be gated, peak=" + juce::String (quiet, 4));
            expect (loud  > 0.20f, "loud tone should pass, peak=" + juce::String (loud, 4));
        }

        beginTest ("gate attack is not a brickwall click");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "gate1: threshold = -20; hyst = 2; attack = 0.02; hold = 0; release = 0.02; range = -80",
                err), err);
            chain.prepare ({ 48000.0, 256, 1 });

            juce::AudioBuffer<float> buf (1, 256);
            buf.clear();
            chain.processBlock (buf);

            float first = 0.f, late = 0.f;
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i, 0.5f);
                chain.processBlock (buf);
                if (b == 0)
                    first = std::abs (buf.getSample (0, 0));
                late = std::abs (buf.getSample (0, 255));
            }
            expect (first < 0.08f, "first sample after close should still be gated, first="
                    + juce::String (first, 3));
            expect (late > 0.3f, "after ~40 ms the gate should be open, late="
                    + juce::String (late, 3));
        }

        beginTest ("comp mix, rms detector, and sub-ms attack");
        {
            juce::String err;
            dsl::SignalChain fast;
            expect (fast.loadScript (
                "comp1: threshold = -12; ratio = 20; attack = 0.0002; release = 0.2; mix = 1; detector = peak",
                err), err);
            fast.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> step (1, 256);
            for (int i = 0; i < 256; ++i)
                step.setSample (0, i, 1.f);
            fast.processBlock (step);
            int halfAt = 256;
            for (int i = 0; i < 256; ++i)
            {
                if (std::abs (step.getSample (0, i)) < 0.6f)
                {
                    halfAt = i;
                    break;
                }
            }
            expect (halfAt < 16,
                    "attack 0.2 ms was floored, half-GR at sample " + juce::String (halfAt));

            auto clickPeak = [] (const char* script)
            {
                dsl::SignalChain chain;
                juce::String e;
                chain.loadScript (script, e);
                chain.prepare ({ 48000.0, 256, 1 });
                juce::AudioBuffer<float> buf (1, 256);
                float pk = 0.f;
                for (int b = 0; b < 8; ++b)
                {
                    buf.clear();
                    buf.setSample (0, 0, 1.f);
                    chain.processBlock (buf);
                    pk = juce::jmax (pk, std::abs (buf.getSample (0, 0)));
                }
                return pk;
            };
            const float peakMode = clickPeak (
                "comp1: threshold = -12; ratio = 10; attack = 0.0002; release = 0.02; detector = peak; mix = 1");
            const float rmsMode = clickPeak (
                "comp1: threshold = -12; ratio = 10; attack = 0.0002; release = 0.02; detector = rms; mix = 1");
            expect (peakMode < 0.93f && rmsMode > 0.95f,
                    "rms detector still follows the peak, peak=" + juce::String (peakMode, 3)
                    + " rms=" + juce::String (rmsMode, 3));

            dsl::SignalChain dry;
            expect (dry.loadScript (
                "comp1: threshold = -40; ratio = 20; attack = 0.0002; release = 0.05; mix = 0",
                err), err);
            dry.prepare ({ 48000.0, 256, 1 });
            const float id = tonePeak (dry, 0.4f, 440.f, 48000.f, 4);
            expect (std::abs (id - 0.4f) < 0.02f,
                    "mix 0 is not dry, peak=" + juce::String (id, 3));
        }

        beginTest ("comp lookahead delays the hit and reports PDC");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "comp1: threshold = -12; ratio = 20; attack = 0.001; release = 0.2; mix = 1; lookahead = 5",
                err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            expectEquals (chain.getIrLatencySamples(), 240);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.f);
            buf.setSample (1, 0, 1.f);
            chain.processBlock (buf);
            expectEquals (TestHelpers::countNonFinite (buf), 0);

            int peakAt = 0;
            float peak = 0.f;
            for (int i = 0; i < 512; ++i)
            {
                const float a = std::abs (buf.getSample (0, i));
                if (a > peak)
                {
                    peak = a;
                    peakAt = i;
                }
            }
            expect (peakAt > 200 && peakAt < 280,
                    "comp lookahead peak not at 5 ms, at=" + juce::String (peakAt)
                    + " peak=" + juce::String (peak, 3));
            expect (std::abs (buf.getSample (0, 0)) < 0.05f,
                    "comp lookahead still emits the hit at sample 0");
        }

        beginTest ("deesser reduces sibilance, not the vowel, and listen is the band");
        {
            auto run = [] (const char* script, float lowAmp, float highAmp, float& lowPeak, float& highMag)
            {
                dsl::SignalChain chain;
                juce::String err;
                const bool ok = chain.loadScript (script, err);
                chain.prepare ({ 48000.0, 256, 1 });
                juce::AudioBuffer<float> buf (1, 256);
                int n = 0;
                for (int b = 0; b < 12; ++b)
                {
                    for (int i = 0; i < 256; ++i, ++n)
                    {
                        const float t = (float) n / 48000.f;
                        const float s = lowAmp * std::sin (2.f * juce::MathConstants<float>::pi * 200.f * t)
                                      + highAmp * std::sin (2.f * juce::MathConstants<float>::pi * 6000.f * t);
                        buf.setSample (0, i, s);
                    }
                    chain.processBlock (buf);
                }
                lowPeak = 0.f;
                float re = 0.f, im = 0.f;
                const float w = 2.f * juce::MathConstants<float>::pi * 6000.f / 48000.f;
                for (int i = 0; i < 256; ++i)
                {
                    const float s = buf.getSample (0, i);
                    lowPeak = juce::jmax (lowPeak, std::abs (s));
                    re += s * std::cos (w * (float) i);
                    im -= s * std::sin (w * (float) i);
                }
                highMag = std::sqrt (re * re + im * im) / 256.f;
                return ok;
            };

            float vowel = 0.f, vowelHf = 0.f, ess = 0.f, essHf = 0.f;
            float mixed = 0.f, mixedHf = 0.f, listenEss = 0.f, listenVowel = 0.f;
            juce::String err;
            dsl::SignalChain probe;
            expect (probe.loadScript (
                "deesser1: freq = 6000; threshold = -6; amount = 1; attack = 0.001; release = 0.05; mix = 1",
                err), err);
            expect (run ("deesser1: freq = 6000; threshold = -6; amount = 1; attack = 0.001; release = 0.05; mix = 1",
                         0.5f, 0.f, vowel, vowelHf));
            expect (run ("deesser1: freq = 6000; threshold = -6; amount = 1; attack = 0.001; release = 0.05; mix = 1",
                         0.f, 0.3f, ess, essHf));
            expect (run ("deesser1: freq = 6000; threshold = -6; amount = 1; attack = 0.001; release = 0.05; mix = 1",
                         0.7f, 0.3f, mixed, mixedHf));
            expect (run ("deesser1: freq = 6000; threshold = -6; amount = 1; listen = on; mix = 1",
                         0.f, 0.3f, listenEss, listenVowel));
            float listenLow = 0.f, listenLowHf = 0.f;
            expect (run ("deesser1: freq = 6000; threshold = -6; amount = 1; listen = on; mix = 1",
                         0.5f, 0.f, listenLow, listenLowHf));
            expect (vowel > 0.35f, "vowel was ducked, peak=" + juce::String (vowel, 3));
            expect (essHf < 0.04f, "sibilance was not reduced, hf=" + juce::String (essHf, 4));
            expect (mixedHf > essHf * 3.f,
                    "relative threshold did not spare HF under a vowel, mixed="
                    + juce::String (mixedHf, 4) + " ess=" + juce::String (essHf, 4));
            expect (listenEss > 0.08f && listenLow < 0.05f,
                    "listen is not the sibilance band, ess=" + juce::String (listenEss, 3)
                    + " vowel=" + juce::String (listenLow, 3));
        }

        beginTest ("transient boosts the hit, cuts the body, and leaves a settled tone alone");
        {
            struct Span { float peak = 0.f; float rms = 0.f; int nonFinite = 0; bool ok = false; };
            auto run = [] (const char* script, auto fill, int peakFrom, int peakTo, int rmsFrom, int rmsTo)
            {
                Span s;
                dsl::SignalChain chain;
                juce::String err;
                s.ok = chain.loadScript (script, err);
                if (! s.ok)
                    return s;
                chain.prepare ({ 48000.0, 256, 1 });
                juce::AudioBuffer<float> buf (1, 256);
                const int total = juce::jmax (peakTo, rmsTo);
                const int blocks = (total + 255) / 256;
                double sum = 0.0;
                int rmsN = 0;
                int n = 0;
                for (int b = 0; b < blocks; ++b)
                {
                    for (int i = 0; i < 256; ++i, ++n)
                        buf.setSample (0, i, fill (n));
                    chain.processBlock (buf);
                    for (int i = 0; i < 256; ++i)
                    {
                        const int k = b * 256 + i;
                        const float y = buf.getSample (0, i);
                        if (! std::isfinite (y))
                            ++s.nonFinite;
                        const float a = std::abs (y);
                        if (k >= peakFrom && k < peakTo && a > s.peak)
                            s.peak = a;
                        if (k >= rmsFrom && k < rmsTo)
                        {
                            sum += (double) y * (double) y;
                            ++rmsN;
                        }
                    }
                }
                s.rms = rmsN > 0 ? (float) std::sqrt (sum / (double) rmsN) : 0.f;
                return s;
            };

            const auto step = [] (int n) { return n < 960 ? 0.6f : 0.f; };
            const auto body = [] (int n) { return n < 3840 ? 0.5f : 0.15f; };
            const auto tone = [] (int n)
            {
                const float t = (float) n / 48000.f;
                return 0.4f * std::sin (2.f * juce::MathConstants<float>::pi * 1000.f * t);
            };
            const auto hitUp = run (
                "transient1: attack = 1; sustain = 0; fast = 0.001; slow = 0.05; mix = 1",
                step, 0, 480, 0, 0);
            const auto hitFlat = run (
                "transient1: attack = 0; sustain = 0; fast = 0.001; slow = 0.05; mix = 1",
                step, 0, 480, 0, 0);
            const auto bodyCut = run (
                "transient1: attack = 0; sustain = -1; fast = 0.001; slow = 0.05; mix = 1",
                body, 0, 0, 4560, 5760);
            const auto bodyFlat = run (
                "transient1: attack = 0; sustain = 0; fast = 0.001; slow = 0.05; mix = 1",
                body, 0, 0, 4560, 5760);
            const auto settled = run (
                "transient1: attack = 1; sustain = 0; fast = 0.005; slow = 0.08; mix = 1",
                tone, 14144, 14400, 0, 0);
            expect (hitUp.ok && hitUp.nonFinite == 0 && hitUp.peak > 1.0f,
                    "hit was not boosted, peak=" + juce::String (hitUp.peak, 3)
                    + " ok=" + juce::String ((int) hitUp.ok));
            expect (hitFlat.peak < 0.75f,
                    "zero attack still punched the step, peak=" + juce::String (hitFlat.peak, 3));
            expect (bodyCut.nonFinite == 0 && bodyCut.rms < 0.10f && bodyFlat.rms > 0.12f,
                    "sustain did not cut the body, cut=" + juce::String (bodyCut.rms, 3)
                    + " flat=" + juce::String (bodyFlat.rms, 3));
            expect (settled.peak > 0.32f && settled.peak < 0.48f,
                    "settled tone was not left alone, peak=" + juce::String (settled.peak, 3));
        }

        beginTest ("utility gain is dB, pan is balance, polarity flips");
        {
            auto peakCh = [] (const char* script, int ch, float amp)
            {
                dsl::SignalChain chain;
                juce::String err;
                if (! chain.loadScript (script, err))
                    return -1.f;
                chain.prepare ({ 48000.0, 256, 2 });
                juce::AudioBuffer<float> buf (2, 256);
                float peak = 0.f;
                for (int b = 0; b < 4; ++b)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        const float t = (float) (b * 256 + i) / 48000.f;
                        const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi * 1000.f * t);
                        buf.setSample (0, i, s);
                        buf.setSample (1, i, s);
                    }
                    chain.processBlock (buf);
                    for (int i = 0; i < 256; ++i)
                        peak = juce::jmax (peak, std::abs (buf.getSample (ch, i)));
                }
                return peak;
            };
            const float ident = peakCh ("utility1: gain = 0; pan = 0; polarity = off", 0, 0.25f);
            const float up = peakCh ("utility1: gain = 6; pan = 0; polarity = off", 0, 0.25f);
            const float left = peakCh ("utility1: gain = 0; pan = -1; polarity = off", 0, 0.4f);
            const float right = peakCh ("utility1: gain = 0; pan = -1; polarity = off", 1, 0.4f);
            dsl::SignalChain flip;
            juce::String err;
            expect (flip.loadScript ("utility1: gain = 0; pan = 0; polarity = on", err), err);
            flip.prepare ({ 48000.0, 64, 1 });
            juce::AudioBuffer<float> dc (1, 64);
            dc.clear();
            dc.setSample (0, 0, 0.3f);
            flip.processBlock (dc);
            expect (ident > 0.23f && ident < 0.27f,
                    "zero utility was not identity, peak=" + juce::String (ident, 3));
            expect (up > 0.47f && up < 0.53f,
                    "+6 dB was not a double, peak=" + juce::String (up, 3));
            expect (left > 0.3f && right < 0.02f,
                    "pan -1 did not keep left and kill right, L=" + juce::String (left, 3)
                    + " R=" + juce::String (right, 3));
            expect (dc.getSample (0, 0) < -0.2f,
                    "polarity did not flip, y=" + juce::String (dc.getSample (0, 0), 3));
        }

        beginTest ("comp makeup raises a signal below threshold");
        {
            dsl::SignalChain plain, boosted;
            juce::String err;
            expect (plain.loadScript (
                "comp1: threshold = -6; ratio = 4; attack = 0.001; release = 0.05", err), err);
            expect (boosted.loadScript (
                "comp1: threshold = -6; ratio = 4; attack = 0.001; release = 0.05; makeup = 6",
                err), err);
            plain.prepare ({ 48000.0, 256, 1 });
            boosted.prepare ({ 48000.0, 256, 1 });
            const float a = tonePeak (plain, 0.1f, 1000.f, 48000.f, 6);
            const float b = tonePeak (boosted, 0.1f, 1000.f, 48000.f, 6);
            expect (b > a * 1.6f, "makeup 6 dB should lift, plain=" + juce::String (a, 3)
                    + " boosted=" + juce::String (b, 3));
        }

        beginTest ("comp hpf lets a 50 Hz tone duck less than 1 kHz");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "comp1: threshold = -24; ratio = 8; attack = 0.001; release = 0.05; hpf = 250",
                err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            dsl::SignalChain chainHi;
            expect (chainHi.loadScript (
                "comp1: threshold = -24; ratio = 8; attack = 0.001; release = 0.05; hpf = 250",
                err), err);
            chainHi.prepare ({ 48000.0, 256, 1 });
            const float bass = tonePeak (chain, 0.5f, 50.f, 48000.f, 10);
            const float mid  = tonePeak (chainHi, 0.5f, 1000.f, 48000.f, 10);
            expect (bass > mid * 1.15f, "HPF detector: 50 Hz should duck less, bass="
                    + juce::String (bass, 3) + " mid=" + juce::String (mid, 3));
        }

        beginTest ("comp source=sidechain ducks from sc not from the input");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "comp1: threshold = -24; ratio = 8; attack = 0.001; release = 0.05; source = sidechain",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });

            juce::AudioBuffer<float> main (2, 256);
            juce::AudioBuffer<float> sc (2, 256);
            sc.clear();
            for (int i = 0; i < 256; ++i)
            {
                main.setSample (0, i, 0.4f);
                main.setSample (1, i, 0.4f);
            }
            for (int b = 0; b < 6; ++b)
                chain.processBlock (main);
            const float dryPeak = std::abs (main.getSample (0, 255));

            for (int i = 0; i < 256; ++i)
            {
                main.setSample (0, i, 0.4f);
                main.setSample (1, i, 0.4f);
                sc.setSample (0, i, 1.0f);
                sc.setSample (1, i, 1.0f);
            }
            chain.setExternalSidechain (sc.getReadPointer (0), sc.getReadPointer (1), 256);
            for (int b = 0; b < 6; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    main.setSample (0, i, 0.4f);
                    main.setSample (1, i, 0.4f);
                }
                chain.processBlock (main);
            }
            const float ducked = std::abs (main.getSample (0, 255));
            expect (dryPeak > 0.3f, "quiet sidechain should leave input, peak="
                    + juce::String (dryPeak, 3));
            expect (ducked < dryPeak * 0.85f, "loud sidechain should duck, dry="
                    + juce::String (dryPeak, 3) + " wet=" + juce::String (ducked, 3));
        }

        beginTest ("limit block parses");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse ("limit1: ceiling = -1; release = 0.08",
                                  blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 1);
            expect (blocks[0].type.startsWith ("limit"));
        }

        beginTest ("limit lookahead delays the peak and reports PDC");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "limit1: ceiling = -6; release = 0.05; lookahead = 5", err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            expectEquals (chain.getIrLatencySamples(), 240);

            juce::AudioBuffer<float> buf (2, 512);
            buf.clear();
            buf.setSample (0, 0, 1.f);
            buf.setSample (1, 0, 1.f);
            chain.processBlock (buf);
            expectEquals (TestHelpers::countNonFinite (buf), 0);

            int peakAt = 0;
            float peak = 0.f;
            for (int i = 0; i < 512; ++i)
            {
                const float a = std::abs (buf.getSample (0, i));
                if (a > peak)
                {
                    peak = a;
                    peakAt = i;
                }
            }
            const float ceilLin = juce::Decibels::decibelsToGain (-6.f);
            expect (peakAt > 200 && peakAt < 280,
                    "lookahead peak not at 5 ms, at=" + juce::String (peakAt));
            expect (peak <= ceilLin + 1.0e-4f,
                    "lookahead exceeded ceiling, peak=" + juce::String (peak, 4));
        }

        beginTest ("limit holds a 0 dBFS sine under the ceiling");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("limit1: ceiling = -1; release = 0.05", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            const float peak = tonePeak (chain, 1.0f, 440.f, 48000.f, 8);
            expect (peak <= 0.92f, "ceiling -1 dB, peak=" + juce::String (peak, 3));
            expect (peak > 0.70f, "should not crush a full-scale sine, peak="
                    + juce::String (peak, 3));
        }

        beginTest ("limit leaves silence and a quiet sine alone");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("limit1: ceiling = -0.3; release = 0.08", err), err);
            chain.prepare ({ 48000.0, 256, 1 });

            juce::AudioBuffer<float> z (1, 256);
            z.clear();
            chain.processBlock (z);
            expectEquals (TestHelpers::countNonFinite (z), 0);
            expect (TestHelpers::peakAbs (z) < 1.0e-6f);

            const float quiet = tonePeak (chain, 0.2f, 1000.f, 48000.f, 4);
            expect (quiet > 0.18f && quiet < 0.22f,
                    "below ceiling should pass, peak=" + juce::String (quiet, 3));
        }

        beginTest ("comp then silence stays finite");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "comp1: threshold = -18; ratio = 6; attack = 0.002; release = 0.08; hpf = 80",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            for (int b = 0; b < 6; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.6f * std::sin (i * 0.11f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlock (buf);
            }
            buf.clear();
            for (int b = 0; b < 20; ++b)
            {
                chain.processBlock (buf);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
        }

        beginTest ("ott block parses");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse (
                "ott1: depth = 0.5; time = 0.3; in = 1; low = 1; mid = 1; high = 1",
                blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 1);
            expect (blocks[0].type.startsWith ("ott"));
        }

        beginTest ("ott lifts a quiet sine and stays finite on a loud one");
        {
            dsl::SignalChain dry, ott;
            juce::String err;
            expect (dry.loadScript ("stage1: y = x", err), err);
            expect (ott.loadScript (
                "ott1: depth = 1; time = 0.25; in = 1.2; low = 1; mid = 1; high = 1",
                err), err);
            dry.prepare ({ 48000.0, 256, 2 });
            ott.prepare ({ 48000.0, 256, 2 });
            const float quietDry = tonePeak (dry, 0.08f, 1000.f, 48000.f, 10);
            const float quietOtt = tonePeak (ott, 0.08f, 1000.f, 48000.f, 10);
            expect (quietOtt > quietDry * 1.15f, "upward should lift a quiet mid, dry="
                    + juce::String (quietDry, 3) + " ott=" + juce::String (quietOtt, 3));

            dsl::SignalChain smash;
            expect (smash.loadScript (
                "ott1: depth = 0.85; time = 0.2; in = 2.0; low = 1; mid = 1; high = 1",
                err), err);
            smash.prepare ({ 48000.0, 256, 2 });
            const float loud = tonePeak (smash, 0.9f, 440.f, 48000.f, 8);
            expect (std::isfinite (loud));
            expect (loud < 2.2f, "OTT should not run away, peak=" + juce::String (loud, 3));
            expect (smash.getMaxTailTime() > 0.02f);
        }

        beginTest ("ott band amount ducks that band, not the other");
        {
            auto peakAt = [] (const char* script, float hz)
            {
                dsl::SignalChain chain;
                juce::String err;
                if (! chain.loadScript (script, err))
                    return -1.f;
                chain.prepare ({ 48000.0, 256, 1 });
                juce::AudioBuffer<float> buf (1, 256);
                float peak = 0.f;
                int n = 0;
                for (int b = 0; b < 16; ++b)
                {
                    for (int i = 0; i < 256; ++i, ++n)
                    {
                        const float s = 0.5f * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) n / 48000.f);
                        buf.setSample (0, i, s);
                    }
                    chain.processBlock (buf);
                    if (b >= 12)
                    {
                        for (int i = 0; i < 256; ++i)
                            peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
                    }
                }
                return peak;
            };
            const char* flat = "ott1: depth = 1; time = 0; in = 1; low = 0; mid = 0; high = 0";
            const char* lowOn = "ott1: depth = 1; time = 0; in = 1; low = 1; mid = 0; high = 0";
            const char* highOn = "ott1: depth = 1; time = 0; in = 1; low = 0; mid = 0; high = 1";
            const float lowFlat = peakAt (flat, 50.f);
            const float lowDuck = peakAt (lowOn, 50.f);
            const float lowSpare = peakAt (highOn, 50.f);
            const float hiFlat = peakAt (flat, 8000.f);
            const float hiDuck = peakAt (highOn, 8000.f);
            const float hiSpare = peakAt (lowOn, 8000.f);
            expect (lowDuck < lowFlat * 0.75f,
                    "low amount did not duck 50 Hz, flat=" + juce::String (lowFlat, 3)
                    + " duck=" + juce::String (lowDuck, 3));
            expect (lowSpare > lowFlat * 0.85f,
                    "high amount ducked the low band, flat=" + juce::String (lowFlat, 3)
                    + " spare=" + juce::String (lowSpare, 3));
            expect (hiDuck < hiFlat * 0.75f,
                    "high amount did not duck 8 kHz, flat=" + juce::String (hiFlat, 3)
                    + " duck=" + juce::String (hiDuck, 3));
            expect (hiSpare > hiFlat * 0.85f,
                    "low amount ducked the high band, flat=" + juce::String (hiFlat, 3)
                    + " spare=" + juce::String (hiSpare, 3));
        }

        beginTest ("limit then silence stays finite");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("limit1: ceiling = -1; release = 0.05", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            (void) tonePeak (chain, 1.0f, 440.f, 48000.f, 4);
            juce::AudioBuffer<float> z (2, 256);
            z.clear();
            for (int b = 0; b < 16; ++b)
            {
                chain.processBlock (z);
                expectEquals (TestHelpers::countNonFinite (z), 0);
                expect (TestHelpers::peakAbs (z) < 1.0e-4f);
            }
        }

        beginTest ("meter is dry and reports peak dB");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("meter1: mode = peak", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                buf.setSample (0, i, 0.5f);
                buf.setSample (1, i, 0.5f);
            }
            chain.processBlock (buf);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (std::abs (buf.getSample (0, 10) - 0.5f) < 1.0e-6f);
            float db = 0.f;
            expect (chain.copyMeterReading ("meter1", db));
            expect (db > -7.f && db < -5.f, "0.5 peak should be about -6 dB, got "
                    + juce::String (db, 2));
        }

        beginTest ("sidechain mix 1 replaces the cable with the extra input");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("sidechain1: mix = 1", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            juce::AudioBuffer<float> sc (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                buf.setSample (0, i, 0.8f);
                buf.setSample (1, i, 0.8f);
                sc.setSample (0, i, 0.1f);
                sc.setSample (1, i, 0.1f);
            }
            chain.setExternalSidechain (sc.getReadPointer (0), sc.getReadPointer (1), 256);
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (std::abs (buf.getSample (0, 200) - 0.1f) < 0.02f,
                    "full SC mix should follow the extra input, y="
                    + juce::String (buf.getSample (0, 200), 3));
        }

        // Contract: external blocks (Comp/Gate/Limit/Widen/Ott/Xover/Ir) inject
        // knobs via cached float* slots. A wrong pair type / map lookup crashes or
        // freezes coeffs — modulated threshold must still move GR.
        beginTest ("contract: knob-modulated external dynamics stay finite and react");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "param a = Ctrl [0, 1]\n"
                "comp1: threshold = map(a,0,1,-60,-6); ratio = 8; attack = 0.001; release = 0.05\n",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });

            std::array<juce::SmoothedValue<float>, Config::kNumUserParams> knobs {};
            for (auto& k : knobs)
            {
                k.reset (48000.0, 0.01);
                k.setCurrentAndTargetValue (0.f);
            }
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobPtrs {};
            for (int i = 0; i < Config::kNumUserParams; ++i)
                knobPtrs[(size_t) i] = &knobs[(size_t) i];

            juce::AudioBuffer<float> buf (2, 256);
            auto fillTone = [&] (float amp)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi
                                                   * 440.f * (float) i / 48000.f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
            };

            knobs[0].setCurrentAndTargetValue (0.f); // open threshold → little GR
            for (int b = 0; b < 8; ++b)
            {
                fillTone (0.5f);
                chain.processBlockSmoothed (buf, knobPtrs);
            }
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            const float openPeak = TestHelpers::peakAbs (buf);

            knobs[0].setCurrentAndTargetValue (1.f); // threshold → -6 dB, heavy GR
            for (int b = 0; b < 16; ++b)
            {
                fillTone (0.5f);
                chain.processBlockSmoothed (buf, knobPtrs);
            }
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            const float closedPeak = TestHelpers::peakAbs (buf);
            expect (openPeak < closedPeak * 0.5f,
                    "a=0 maps to threshold -60 (more GR) vs a=1 → -6: open="
                    + juce::String (openPeak, 4) + " closed=" + juce::String (closedPeak, 4));
        }

        // Contract: MIDI vars write through HotSlots pointers after prepare.
        beginTest ("contract: setMidiVariables reaches env via hot slots");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "env1: type = peak; attack = 0.001; release = 0.05\n"
                "stage1: y = x * (0.1 + 0.9 * midi_gate)\n",
                err), err);
            chain.prepare ({ 48000.0, 128, 1 });

            MidiVariableMapper midi;
            juce::MidiBuffer notes;
            notes.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.processMidi (notes);
            chain.setMidiVariables (midi);

            juce::AudioBuffer<float> buf (1, 128);
            for (int i = 0; i < 128; ++i)
                buf.setSample (0, i, 0.5f);
            chain.processBlock (buf);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            const float gatedOn = TestHelpers::peakAbs (buf);
            expect (gatedOn > 0.35f, "midi_gate=1 should pass most of the tone, peak="
                    + juce::String (gatedOn, 3));

            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            midi.processMidi (off);
            chain.setMidiVariables (midi);
            for (int i = 0; i < 128; ++i)
                buf.setSample (0, i, 0.5f);
            for (int b = 0; b < 4; ++b)
                chain.processBlock (buf);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            const float gatedOff = TestHelpers::peakAbs (buf);
            expect (gatedOff < gatedOn * 0.5f,
                    "midi_gate=0 should attenuate, on=" + juce::String (gatedOn, 3)
                    + " off=" + juce::String (gatedOff, 3));
        }

        beginTest ("pitch block parses and aliases");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse ("pitch1: semitones = 7; mix = 1; formant = 1; ceiling = -0.3",
                                  blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 1);
            expectEquals (blocks[0].type, juce::String ("pitch"));
            blocks.clear();
            expect (parser.parse ("pshift1: shift = -12; sync = 1/8",
                                  blocks, aliases, params, err), err);
            expectEquals (blocks[0].type, juce::String ("pitch"));
        }

        beginTest ("pitch unity mix stays finite and near dry after latency");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "pitch1: semitones = 0; mix = 1; formant = 1; ceiling = -0.3", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            // Flush STFT latency (~fft - hop), then measure.
            const float peak = tonePeak (chain, 1.0f, 440.f, 48000.f, 24);
            juce::AudioBuffer<float> z (2, 256);
            z.clear();
            chain.processBlock (z);
            expectEquals (TestHelpers::countNonFinite (z), 0);
            expect (std::isfinite (peak), "unity pitch peak=" + juce::String (peak, 3));
            expect (chain.getIrLatencySamples() > 0, "pitch must report STFT latency");
        }

        beginTest ("pitch ceiling soft-caps wet peaks");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "pitch1: semitones = 0; mix = 1; formant = 1; ceiling = -6", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            const float peak = tonePeak (chain, 1.0f, 440.f, 48000.f, 16);
            const float ceilLin = juce::Decibels::decibelsToGain (-6.f);
            expect (peak <= ceilLin * 1.15f + 0.05f,
                    "ceiling -6 dB, peak=" + juce::String (peak, 3));
            dsl::SignalChain deep;
            expect (deep.loadScript (
                "pitch1: semitones = 0; mix = 1; formant = 1; ceiling = -18", err), err);
            deep.prepare ({ 48000.0, 256, 2 });
            const float deepPeak = tonePeak (deep, 1.0f, 440.f, 48000.f, 16);
            expect (deepPeak <= juce::Decibels::decibelsToGain (-18.f) * 1.15f + 0.02f,
                    "ceiling -18 dB was not the published range, peak=" + juce::String (deepPeak, 3));
            expect (deepPeak < peak * 0.5f,
                    "deeper ceiling did not sit under -6, deep=" + juce::String (deepPeak, 3)
                    + " shallow=" + juce::String (peak, 3));
        }

        beginTest ("comp ceiling soft-caps makeup boost");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "comp1: threshold = -6; ratio = 1.1; attack = 0.001; release = 0.05; makeup = 12; ceiling = -6",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            const float peak = tonePeak (chain, 0.5f, 1000.f, 48000.f, 6);
            // A named ceiling is a bound, not the onset of a curve approaching twice the ceiling.
            expect (peak <= juce::Decibels::decibelsToGain (-6.f) && peak > 0.45f,
                    "comp ceiling should hold makeup, peak=" + juce::String (peak, 3));
        }

        beginTest ("gate ceiling soft-caps open path");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "gate1: threshold = -60; range = 0; attack = 0.001; release = 0.02; ceiling = -6",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            const float peak = tonePeak (chain, 1.0f, 200.f, 48000.f, 6);
            expect (peak < 0.85f && peak > 0.45f,
                    "gate ceiling should hold, peak=" + juce::String (peak, 3));
        }

        beginTest ("chainwide soft-ceiling bounds overs and preserves low-level audio");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("stage1: y = x * 2", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 0.8f);
            chain.processBlock (buf);
            const float peak = TestHelpers::peakAbs (buf);
            // Internal bus guard preserves 0 dBFS and compresses overs into 6 dB headroom.
            expect (peak > 1.0f && peak < 1.6f,
                    "overs soft-shaped, peak=" + juce::String (peak, 3));
            expectEquals (TestHelpers::countNonFinite (buf), 0);

            expect (chain.loadScript ("stage1: y = x * 0.5", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 0.5f);
            chain.processBlock (buf);
            const float quiet = TestHelpers::peakAbs (buf);
            expect (quiet > 0.24f && quiet < 0.26f,
                    "sub-FS chain must stay untouched, peak=" + juce::String (quiet, 3));
        }
    }

private:
    static float tonePeak (dsl::SignalChain& chain, float amp, float hz, float sr, int blocks)
    {
        juce::AudioBuffer<float> buf (2, 256);
        float peak = 0.f;
        int n = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i, ++n)
            {
                const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) n / sr);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            chain.processBlock (buf);
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
        }
        return peak;
    }
};
