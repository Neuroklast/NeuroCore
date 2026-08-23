#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "../src/dsp/DSPUtils.h"
#include "TestHelpers.h"
#include <cmath>

class ModulationBlocksTest : public juce::UnitTest
{
public:
    ModulationBlocksTest() : juce::UnitTest ("ModulationBlocksTest", "DSP") {}

    void runTest() override
    {
        beginTest ("allpass / phaser / flanger / env unit parse");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;

            expect (parser.parse ("filter1: type = allpass; cutoff = 800",
                                  blocks, aliases, params, err), err);
            expectEquals (blocks[0].type, juce::String ("filter"));
            expectEquals (blocks[0].args.at ("type"), juce::String ("allpass"));

            blocks.clear();
            expect (parser.parse ("apf1: cutoff = 400", blocks, aliases, params, err), err);
            expectEquals (blocks[0].type, juce::String ("filter"));
            expectEquals (blocks[0].args.at ("type"), juce::String ("allpass"));

            blocks.clear();
            expect (parser.parse (
                "phaser1: stages = 6; rate = 0.4; depth = 0.7; center = 800; feedback = 0.3; mix = 0.5",
                blocks, aliases, params, err), err);
            expectEquals (blocks[0].type, juce::String ("phaser"));

            blocks.clear();
            expect (parser.parse (
                "flange1: rate = 0.2; depth = 0.8; delay = 2; feedback = 0.5; mix = 0.5; invert = on",
                blocks, aliases, params, err), err);
            expectEquals (blocks[0].type, juce::String ("flanger"));

            blocks.clear();
            expect (parser.parse ("env1: type = rms; unit = db; attack = 0.01; release = 0.1",
                                  blocks, aliases, params, err), err);
            expectEquals (blocks[0].type, juce::String ("env"));
            expectEquals (blocks[0].args.at ("unit"), juce::String ("db"));
        }

        beginTest ("allpass is unity-gain and two stages + dry notch at fc");
        {
            dsl::SignalChain dry;
            juce::String err;
            expect (dry.loadScript ("stage1: y = x", err), err);
            dry.prepare ({ 48000.0, 256, 2 });

            dsl::SignalChain ap;
            expect (ap.loadScript ("filter1: type = allpass; cutoff = 1000; resonance = 0.7", err), err);
            ap.prepare ({ 48000.0, 256, 2 });
            const float inPk = tonePeak (dry, 0.5f, 1000.f, 48000.f, 8);
            const float apPk = tonePeak (ap, 0.5f, 1000.f, 48000.f, 8);
            expectEquals (TestHelpers::countNonFinite (lastBuf), 0);
            expect (apPk > inPk * 0.85f && apPk < inPk * 1.15f,
                    "allpass peak should match dry, dry=" + juce::String (inPk, 3)
                    + " ap=" + juce::String (apPk, 3));

            dsl::SignalChain apImp;
            expect (apImp.loadScript ("filter1: type = allpass; cutoff = 1000", err), err);
            apImp.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> imp (1, 256);
            imp.clear();
            imp.setSample (0, 0, 1.f);
            apImp.processBlock (imp);
            const float y0 = imp.getSample (0, 0);
            const float a0 = DSPUtils::onePoleAllpassA (1000.f, 48000.f);
            expect (std::abs (y0 - a0) < 0.02f,
                    "allpass impulse y[0] should be a, a=" + juce::String (a0, 4)
                    + " y0=" + juce::String (y0, 4));

            dsl::SignalChain ap2;
            expect (ap2.loadScript (
                "filter1: type = allpass; cutoff = 1000\n"
                "filter2: type = allpass; cutoff = 1000\n",
                err), err);
            ap2.prepare ({ 48000.0, 256, 2 });
            const float atFc = mixedDryPeak (ap2, 0.5f, 1000.f, 48000.f, 12);
            const float oracle = oracleTwoPoleMix (0.5f, 1000.f, 48000.f);
            dsl::SignalChain ap2b;
            expect (ap2b.loadScript (
                "filter1: type = allpass; cutoff = 1000\n"
                "filter2: type = allpass; cutoff = 1000\n",
                err), err);
            ap2b.prepare ({ 48000.0, 256, 2 });
            const float offFc = mixedDryPeak (ap2b, 0.5f, 200.f, 48000.f, 12);
            expect (oracle < 0.15f,
                    "oracle 2-pole+dry must notch at 1 kHz, oracle=" + juce::String (oracle, 4));
            expect (atFc < offFc * 0.35f,
                    "2-pole allpass+dry must notch at fc, at=" + juce::String (atFc, 4)
                    + " off=" + juce::String (offFc, 4) + " oracle=" + juce::String (oracle, 4));
        }

        beginTest ("phaser two-stage mix 0.5 notches at center when LFO is frozen");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "phaser1: stages = 2; rate = 0; depth = 0; center = 1000; feedback = 0; mix = 0.5",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            const float atFc = settledPeak (chain, 0.5f, 1000.f, 48000.f, 12);
            dsl::SignalChain chain2;
            expect (chain2.loadScript (
                "phaser1: stages = 2; rate = 0; depth = 0; center = 1000; feedback = 0; mix = 0.5",
                err), err);
            chain2.prepare ({ 48000.0, 256, 2 });
            const float offFc = settledPeak (chain2, 0.5f, 200.f, 48000.f, 12);
            expectEquals (TestHelpers::countNonFinite (lastBuf), 0);
            expect (atFc < offFc * 0.35f,
                    "phaser must notch at center, at=" + juce::String (atFc, 4)
                    + " off=" + juce::String (offFc, 4));
        }

        beginTest ("flanger invert comb notches at 1/(2 delay)");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "flanger1: rate = 0; depth = 0; delay = 1; feedback = 0; mix = 0.5; invert = off",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            const float notch = settledPeak (chain, 0.5f, 500.f, 48000.f, 12);
            dsl::SignalChain chain2;
            expect (chain2.loadScript (
                "flanger1: rate = 0; depth = 0; delay = 1; feedback = 0; mix = 0.5; invert = off",
                err), err);
            chain2.prepare ({ 48000.0, 256, 2 });
            const float lobe = settledPeak (chain2, 0.5f, 250.f, 48000.f, 12);
            expectEquals (TestHelpers::countNonFinite (lastBuf), 0);
            expect (notch < lobe * 0.4f,
                    "1 ms flanger notches 500 Hz, notch=" + juce::String (notch, 4)
                    + " lobe=" + juce::String (lobe, 4));
        }

        beginTest ("env unit=db reports dBFS, lin stays 0-1");
        {
            juce::String err;
            // Scale dB into ±1 so the chainwide |x|>1 soft-shape does not crush it.
            const float fs = dcEnv ("env1: type = peak; unit = db; attack = 0.0001; release = 0.0001\n"
                                    "stage1: y = env1 * 0.01\n",
                                    1.0f);
            const float quiet = dcEnv ("env1: type = peak; unit = db; attack = 0.0001; release = 0.0001\n"
                                       "stage1: y = env1 * 0.01\n",
                                       0.1f);
            expect (std::abs (fs) < 0.03f,
                    "full-scale peak env dB*0.01 should be ~0, got " + juce::String (fs, 4));
            expect (quiet < -0.16f && quiet > -0.24f,
                    "0.1 peak should be ~-0.20 (dB*0.01), got " + juce::String (quiet, 4));

            const float lin = dcEnv ("env1: type = peak; attack = 0.0001; release = 0.0001\n"
                                     "stage1: y = env1\n",
                                     1.0f);
            expect (lin > 0.9f && lin < 1.05f,
                    "lin env of FS peak stays ~1, got " + juce::String (lin, 3));
        }

        beginTest ("env dB + stage builds downward compression without comp");
        {
            juce::String err;
            dsl::SignalChain raw;
            expect (raw.loadScript ("stage1: y = x", err), err);
            raw.prepare ({ 48000.0, 256, 2 });
            const float loudRaw = tonePeak (raw, 0.9f, 200.f, 48000.f, 8);

            dsl::SignalChain diy;
            expect (diy.loadScript (
                "env1: type = peak; unit = db; attack = 0.0002; release = 0.04\n"
                "stage1: y = x * pow(10, -max(0, env1 - (-6)) * 0.75 / 20)\n",
                err), err);
            diy.prepare ({ 48000.0, 256, 2 });
            const float loudDiy = tonePeak (diy, 0.9f, 200.f, 48000.f, 12);
            expect (loudDiy < loudRaw * 0.85f,
                    "DIY dB env VCA should reduce a hot tone, raw=" + juce::String (loudRaw, 3)
                    + " diy=" + juce::String (loudDiy, 3));

            dsl::SignalChain quietRaw;
            expect (quietRaw.loadScript ("stage1: y = x", err), err);
            quietRaw.prepare ({ 48000.0, 256, 2 });
            const float qRaw = tonePeak (quietRaw, 0.08f, 200.f, 48000.f, 8);
            dsl::SignalChain quietDiy;
            expect (quietDiy.loadScript (
                "env1: type = peak; unit = db; attack = 0.0002; release = 0.04\n"
                "stage1: y = x * pow(10, -max(0, env1 - (-6)) * 0.75 / 20)\n",
                err), err);
            quietDiy.prepare ({ 48000.0, 256, 2 });
            const float qDiy = tonePeak (quietDiy, 0.08f, 200.f, 48000.f, 12);
            expect (qDiy > qRaw * 0.85f,
                    "below threshold the DIY comp must leave the tone, raw="
                    + juce::String (qRaw, 3) + " diy=" + juce::String (qDiy, 3));
        }
    }

private:
    juce::AudioBuffer<float> lastBuf { 2, 256 };

    float tonePeak (dsl::SignalChain& chain, float amp, float hz, float sr, int blocks)
    {
        lastBuf.setSize (2, 256);
        float peak = 0.f;
        int n = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i, ++n)
            {
                const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) n / sr);
                lastBuf.setSample (0, i, s);
                lastBuf.setSample (1, i, s);
            }
            chain.processBlock (lastBuf);
            peak = juce::jmax (peak, TestHelpers::peakAbs (lastBuf));
        }
        return peak;
    }

    static float oracleTwoPoleMix (float amp, float hz, float sr)
    {
        const float a = DSPUtils::onePoleAllpassA (1000.f, sr);
        float z1 = 0.f, z2 = 0.f;
        float peak = 0.f;
        const int n = 4096;
        for (int i = 0; i < n; ++i)
        {
            const float x = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) i / sr);
            const float y1 = DSPUtils::onePoleAllpassTick (x, a, z1);
            const float y2 = DSPUtils::onePoleAllpassTick (y1, a, z2);
            const float m = 0.5f * x + 0.5f * y2;
            if (i >= n / 2)
                peak = juce::jmax (peak, std::abs (m));
        }
        return peak;
    }

    float mixedDryPeak (dsl::SignalChain& chain, float amp, float hz, float sr, int blocks)
    {
        lastBuf.setSize (2, 256);
        juce::AudioBuffer<float> dry (2, 256);
        int n = 0;
        float peak = 0.f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i, ++n)
            {
                const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) n / sr);
                lastBuf.setSample (0, i, s);
                lastBuf.setSample (1, i, s);
                dry.setSample (0, i, s);
                dry.setSample (1, i, s);
            }
            chain.processBlock (lastBuf);
            if (b < blocks / 2)
                continue;
            for (int i = 0; i < 256; ++i)
            {
                const float m = 0.5f * dry.getSample (0, i) + 0.5f * lastBuf.getSample (0, i);
                peak = juce::jmax (peak, std::abs (m));
            }
        }
        return peak;
    }

    float settledPeak (dsl::SignalChain& chain, float amp, float hz, float sr, int blocks)
    {
        lastBuf.setSize (2, 256);
        int n = 0;
        float peak = 0.f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i, ++n)
            {
                const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) n / sr);
                lastBuf.setSample (0, i, s);
                lastBuf.setSample (1, i, s);
            }
            chain.processBlock (lastBuf);
            if (b >= blocks / 2)
                peak = juce::jmax (peak, TestHelpers::peakAbs (lastBuf));
        }
        return peak;
    }

    float dcEnv (const juce::String& script, float dc)
    {
        dsl::SignalChain chain;
        juce::String err;
        expect (chain.loadScript (script, err), err);
        chain.prepare ({ 48000.0, 256, 1 });
        juce::AudioBuffer<float> buf (1, 256);
        float last = 0.f;
        for (int b = 0; b < 6; ++b)
        {
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, dc);
            chain.processBlock (buf);
            last = buf.getSample (0, 255);
        }
        expectEquals (TestHelpers::countNonFinite (buf), 0);
        return last;
    }
};
