#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "TestHelpers.h"
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
