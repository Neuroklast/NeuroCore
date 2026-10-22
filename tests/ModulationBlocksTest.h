#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "../src/core/MidiVariableMapper.h"
#include "../src/dsp/DSPUtils.h"
#include "TestHelpers.h"
#include <cmath>

#if NK_ALLOCATION_PROBE
extern "C" void nkBeginAllocationProbe();
extern "C" unsigned nkEndAllocationProbe();
#endif

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

        beginTest ("chorus moves sine zero-crossings; mix 0 is identity");
        {
            // Amplitude modulation keeps zero-crossings put. A modulated delay does not.
            dsl::SignalChain wet;
            juce::String err;
            expect (wet.loadScript (
                "chorus1: rate = 1.5; depth = 1; delay = 20; voices = 2; mix = 1; width = 0",
                err), err);
            wet.prepare ({ 48000.0, 512, 2 });

            dsl::SignalChain dry;
            expect (dry.loadScript (
                "chorus1: rate = 1.5; depth = 1; delay = 20; voices = 2; mix = 0; width = 0",
                err), err);
            dry.prepare ({ 48000.0, 512, 2 });

            const float hz = 200.f;
            const float amp = 0.4f;
            const float w = 2.f * juce::MathConstants<float>::pi * hz;
            juce::AudioBuffer<float> buf (2, 512);
            juce::AudioBuffer<float> id (2, 512);
            float idErr = 0.f;
            float prev = 0.f;
            int prevZc = -1;
            int minGap = 100000;
            int maxGap = 0;
            int gaps = 0;
            int n = 0;
            for (int b = 0; b < 80; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = amp * std::sin (w * (float) (b * 512 + i) / 48000.f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                    id.setSample (0, i, s);
                    id.setSample (1, i, s);
                }
                wet.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                dry.processBlockSmoothed (id, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
                if (b < 4)
                    continue;
                for (int i = 0; i < 512; ++i)
                {
                    const float s = amp * std::sin (w * (float) (b * 512 + i) / 48000.f);
                    idErr = juce::jmax (idErr, std::abs (id.getSample (0, i) - s));
                    const float y = buf.getSample (0, i);
                    if (prev <= 0.f && y > 0.f && prevZc >= 0)
                    {
                        const int gap = n - prevZc;
                        if (gap > 40 && gap < 800)
                        {
                            minGap = juce::jmin (minGap, gap);
                            maxGap = juce::jmax (maxGap, gap);
                            ++gaps;
                        }
                        prevZc = n;
                    }
                    else if (prev <= 0.f && y > 0.f)
                    {
                        prevZc = n;
                    }
                    prev = y;
                    ++n;
                }
            }
            expect (idErr < 1.0e-3f, "chorus mix 0 is not identity, err=" + juce::String (idErr, 5));
            expect (gaps > 20 && maxGap - minGap > 4,
                    "chorus did not move zero-crossings, gaps=" + juce::String (gaps)
                    + " span=" + juce::String (maxGap - minGap));
        }

        beginTest ("filter resonance 4 is the ceiling, 10 is not peakier");
        {
            auto peakAt = [] (float q)
            {
                dsl::SignalChain chain;
                juce::String err;
                const auto script = "filter1: type = lowpass; cutoff = 1000; resonance = " + juce::String (q, 1);
                if (! chain.loadScript (script, err))
                    return -1.f;
                chain.prepare ({ 48000.0, 256, 1 });
                juce::AudioBuffer<float> buf (1, 256);
                float peak = 0.f;
                for (int b = 0; b < 12; ++b)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        const float t = (float) (b * 256 + i) / 48000.f;
                        buf.setSample (0, i, 0.05f * std::sin (2.f * juce::MathConstants<float>::pi * 1000.f * t));
                    }
                    chain.processBlock (buf);
                    if (b >= 6)
                        peak = juce::jmax (peak, TestHelpers::peakAbs (buf));
                }
                return peak;
            };
            const float q35 = peakAt (3.5f);
            const float q4 = peakAt (4.f);
            const float q10 = peakAt (10.f);
            expect (q4 > q35 * 1.05f,
                    "Q 4 was swallowed by a lower clamp, q4=" + juce::String (q4, 3)
                    + " q35=" + juce::String (q35, 3));
            expect (q10 > 0.f && std::abs (q10 - q4) / juce::jmax (1.0e-6f, q4) < 0.08f,
                    "Q 10 was peakier than the ceiling, q10=" + juce::String (q10, 3)
                    + " q4=" + juce::String (q4, 3));
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

        beginTest ("random control source modulates a stage without audio");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "random1: rate = 1000; min = 0.5; max = 0.5; mix = 1\n"
                "stage1: y = x * random1\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i, 0.5f);
                chain.processBlock (buf);
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
            expectWithinAbsoluteError (buf.getSample (0, 255), 0.25f, 1.0e-3f);
        }

        beginTest ("control depth and polarity scale the published value");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "random1: rate = 1000; min = 0.5; max = 0.5; mix = 1; depth = 0.5; polarity = invert\n"
                "stage1: y = x * random1\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i, 0.5f);
                chain.processBlock (buf);
            }
            expectWithinAbsoluteError (buf.getSample (0, 255), -0.125f, 1.0e-3f);
        }

        beginTest ("clearRuntimeState resets the published control value");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "random1: rate = 1000; min = 1; max = 1; mix = 1\n"
                "stage1: y = x * random1\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int b = 0; b < 4; ++b)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i, 1.0f);
                chain.processBlock (buf);
            }
            expectWithinAbsoluteError (buf.getSample (0, 255), 1.0f, 1.0e-3f);
            chain.clearRuntimeState();
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expect (std::abs (buf.getSample (0, 0)) < 1.0e-6f,
                    "reset must clear the published control value");
        }

        beginTest ("modulator is not an audio effect");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "random1: rate = 1000; min = -1; max = 1; mix = 1\n"
                "stage1: y = x\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 0.3f);
            chain.processBlock (buf);
            for (int i = 32; i < 256; ++i)
                expectWithinAbsoluteError (buf.getSample (0, i), 0.3f, 1.0e-4f);
        }

        beginTest ("modmath combines two named control sources");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "random1: rate = 1000; min = 0.5; max = 0.5; mix = 1\n"
                "random2: rate = 1000; min = 0.25; max = 0.25; mix = 1\n"
                "modmath1: ina = random1; inb = random2; mode = add; amount = 1; mix = 1\n"
                "stage1: y = x * modmath1\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int b = 0; b < 4; ++b)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i, 0.5f);
                chain.processBlock (buf);
            }
            expectWithinAbsoluteError (buf.getSample (0, 255), 0.375f, 1.0e-3f);
        }

        beginTest ("modulation cycle is rejected at load");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (! chain.loadScript (
                "random1: rate = 10; min = 0; max = 1; mix = 1\n"
                "modmath1: ina = modmath2; inb = random1; mode = add\n"
                "modmath2: ina = modmath1; inb = random1; mode = add\n", err));
            expect (err.containsIgnoreCase ("cycle"), err);
        }

        beginTest ("control source modulates a filter cutoff");
        {
            auto peakAt = [] (float value)
            {
                dsl::SignalChain chain;
                juce::String err;
                const auto v = juce::String (value, 2);
                if (! chain.loadScript (
                        "random1: rate = 1000; min = " + v + "; max = " + v + "; mix = 1\n"
                        "filter1: type = lowpass; cutoff = 200 + random1 * 5000; resonance = 0.7\n", err))
                    return -1.f;
                chain.prepare ({ 48000.0, 256, 1 });
                juce::AudioBuffer<float> buf (1, 256);
                float peak = 0.f;
                int n = 0;
                for (int b = 0; b < 24; ++b)
                {
                    for (int i = 0; i < 256; ++i, ++n)
                        buf.setSample (0, i, 0.1f * std::sin (
                            2.f * juce::MathConstants<float>::pi * 5000.f * (float) n / 48000.f));
                    chain.processBlock (buf);
                    if (b >= 12)
                        peak = juce::jmax (peak, TestHelpers::peakAbs (buf));
                }
                return peak;
            };
            const float low = peakAt (0.0f);
            const float high = peakAt (1.0f);
            expect (low > 0.f && high > low * 2.f,
                    "open cutoff must pass more 5 kHz than closed, low="
                    + juce::String (low, 4) + " high=" + juce::String (high, 4));
        }

        beginTest ("stepseq plays the configured step values");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 4; rate = 100; gate = 1; mix = 1; values = 1, 0, 0.5, 0\n"
                "stage1: y = x * stepseq1\n", err), err);
            chain.prepare ({ 48000.0, 1920, 1 });
            juce::AudioBuffer<float> buf (1, 1920);
            for (int i = 0; i < 1920; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 1.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 490), 0.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 970), 0.5f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 1450), 0.0f, 1.0e-3f);
        }

        beginTest ("more than 64 simultaneous control generators render");
        {
            juce::String script;
            for (int i = 1; i <= 80; ++i)
                script += "stepseq" + juce::String (i)
                    + ": steps = 1; rate = 100; values = 0.5\n";
            script += "stage1: y = x * stepseq80\n";
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript (script, error), error);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> audio (1, 256);
            for (int sample = 0; sample < 256; ++sample)
                audio.setSample (0, sample, 1.f);
            chain.processBlock (audio);
            expectWithinAbsoluteError (audio.getSample (0, 200), 0.5f, 1.0e-3f);
           #if NK_ALLOCATION_PROBE
            nkBeginAllocationProbe();
            for (int iteration = 0; iteration < 16; ++iteration)
            {
                for (int sample = 0; sample < 256; ++sample)
                    audio.setSample (0, sample, 1.f);
                chain.processBlock (audio);
            }
            const auto allocations = nkEndAllocationProbe();
            expectEquals ((int) allocations, 0,
                          "80 generators must not allocate during processBlock");
           #endif
        }

        beginTest ("long pure modulation dependency chain has no 64-node cutoff");
        {
            juce::String script = "stepseq1: steps = 1; values = 0.5\n";
            for (int i = 2; i <= 70; ++i)
                script += "modmath" + juce::String (i)
                    + ": ina = " + (i == 2 ? juce::String ("stepseq1")
                                        : juce::String ("modmath") + juce::String (i - 1))
                    + "; mode = add; amount = 1; mix = 1\n";
            script += "stage1: y = modmath70\n";
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript (script, error), error);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> audio (1, 256);
            audio.clear();
            chain.processBlock (audio);
            expectWithinAbsoluteError (audio.getSample (0, 200), 0.5f, 1.0e-3f);
        }

        beginTest ("stepseq supports 64 steps and wraps");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 64; rate = 100; gate = 1; mix = 1; values = 1\n"
                "stage1: y = x * stepseq1\n", err), err);
            chain.prepare ({ 48000.0, 30800, 1 });
            juce::AudioBuffer<float> buf (1, 30800);
            for (int i = 0; i < 30800; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 1.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 63 * 480 + 10), 0.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 64 * 480 + 10), 1.0f, 1.0e-3f);
        }

        beginTest ("stepseq reverse direction and mutes");
        {
            dsl::SignalChain reverse;
            juce::String err;
            expect (reverse.loadScript (
                "stepseq1: steps = 3; rate = 100; direction = reverse; values = 1, 0.5, 0\n"
                "stage1: y = x * stepseq1\n", err), err);
            reverse.prepare ({ 48000.0, 1440, 1 });
            juce::AudioBuffer<float> buf (1, 1440);
            for (int i = 0; i < 1440; ++i)
                buf.setSample (0, i, 1.0f);
            reverse.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 0.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 490), 0.5f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 970), 1.0f, 1.0e-3f);

            dsl::SignalChain muted;
            expect (muted.loadScript (
                "stepseq1: steps = 2; rate = 100; mutes = 0, 1; values = 1, 1\n"
                "stage1: y = x * stepseq1\n", err), err);
            muted.prepare ({ 48000.0, 1440, 1 });
            for (int i = 0; i < 1440; ++i)
                buf.setSample (0, i, 1.0f);
            muted.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 1.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 490), 0.0f, 1.0e-3f);
        }

        beginTest ("stepseq probability and gate length");
        {
            dsl::SignalChain none;
            juce::String err;
            expect (none.loadScript (
                "stepseq1: steps = 1; rate = 100; probability = 0; values = 1\n"
                "stage1: y = x * stepseq1\n", err), err);
            none.prepare ({ 48000.0, 480, 1 });
            juce::AudioBuffer<float> buf (1, 480);
            for (int i = 0; i < 480; ++i)
                buf.setSample (0, i, 1.0f);
            none.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 0.0f, 1.0e-3f);

            dsl::SignalChain gated;
            expect (gated.loadScript (
                "stepseq1: steps = 1; rate = 100; gate = 0.5; values = 1\n"
                "stage1: y = x * stepseq1\n", err), err);
            gated.prepare ({ 48000.0, 480, 1 });
            for (int i = 0; i < 480; ++i)
                buf.setSample (0, i, 1.0f);
            gated.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 1.0f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 250), 0.0f, 1.0e-3f);
        }

        beginTest ("stepseq syncs to host BPM and transport");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 2; sync = 1/4; values = 1, 0\n"
                "stage1: y = x * stepseq1\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            chain.setTempo (120.0, 0.0, true);
            juce::AudioBuffer<float> buf (1, 256);
            for (int b = 0; b < 160; ++b)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i, 1.0f);
                chain.processBlock (buf);
                if (b == 10)
                    expectWithinAbsoluteError (buf.getSample (0, 255), 1.0f, 1.0e-3f);
            }
            expectWithinAbsoluteError (buf.getSample (0, 255), 0.0f, 1.0e-3f);
            chain.setTempo (120.0, 0.0, false);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 255), 0.0f, 1.0e-3f);
        }

        beginTest ("host-sync swing respects beat boundaries and negative PPQ");
        {
            for (double ppq : { 0.799, -0.201 })
            {
                dsl::SignalChain chain;
                juce::String err;
                expect (chain.loadScript (
                    "stepseq1: steps = 2; sync = 1/8; swing = 0.6; "
                    "gate = 1; mix = 1; values = 1, 0\n"
                    "stage1: y = x * stepseq1\n", err), err);
                chain.prepare ({ 48000.0, 128, 1 });
                chain.setTempo (120.0, ppq, true);
                juce::AudioBuffer<float> audio (1, 128);
                for (int i = 0; i < 128; ++i)
                    audio.setSample (0, i, 1.0f);
                chain.processBlock (audio);
                // At 120 BPM, PPQ advances by 1/24000 per sample. The
                // swung first step ends at 0.8 PPQ: sample 10 precedes it,
                // sample 64 follows it, including after a negative-PPQ seek.
                expectWithinAbsoluteError (audio.getSample (0, 10), 1.f, 1.0e-4f);
                expectWithinAbsoluteError (audio.getSample (0, 64), 0.f, 1.0e-4f);
            }
        }

        beginTest ("legacy step0..step3 args still drive the sequencer");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 2; rate = 100; step0 = 0.5; step1 = 0; mix = 1\n"
                "stage1: y = x * stepseq1\n", err), err);
            chain.prepare ({ 48000.0, 960, 1 });
            juce::AudioBuffer<float> buf (1, 960);
            for (int i = 0; i < 960; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 10), 0.5f, 1.0e-3f);
            expectWithinAbsoluteError (buf.getSample (0, 490), 0.0f, 1.0e-3f);
        }

        beginTest ("stepseq publishes its live step for the circuit");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 2; rate = 100; values = 1, 0\n"
                "stage1: y = x * stepseq1\n", err), err);
            chain.prepare ({ 48000.0, 700, 1 });
            juce::AudioBuffer<float> buf (1, 700);
            for (int i = 0; i < 700; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            juce::Array<juce::var> rows;
            chain.appendClipPeaks (rows);
            int step = -1;
            int steps = 0;
            for (const auto& row : rows)
                if (auto* o = row.getDynamicObject())
                    if (o->getProperty ("id").toString() == "stepseq1")
                    {
                        step = (int) o->getProperty ("step");
                        steps = (int) o->getProperty ("steps");
                    }
            expectEquals (step, 1);
            expectEquals (steps, 2);
        }

        beginTest ("adsr reaches attack, decay and sustain with exact linear timing");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "adsr1: attack = 0.01; decay = 0.01; sustain = 0.5; release = 0.01; retrigger = on; gate = audio\n"
                "stage1: y = adsr1\n", err), err);
            chain.prepare ({ 48000.0, 2000, 1 });
            juce::AudioBuffer<float> buf (1, 2000);
            for (int i = 0; i < 2000; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 240), 0.5f, 0.02f);
            expectWithinAbsoluteError (buf.getSample (0, 480), 1.0f, 0.02f);
            expectWithinAbsoluteError (buf.getSample (0, 720), 0.75f, 0.02f);
            expectWithinAbsoluteError (buf.getSample (0, 1200), 0.5f, 0.02f);
        }

        beginTest ("adsr release and retrigger are sample-accurate");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "adsr1: attack = 0.01; decay = 0.001; sustain = 1; release = 0.01; retrigger = on; gate = audio\n"
                "stage1: y = adsr1\n", err), err);
            chain.prepare ({ 48000.0, 780, 1 });
            juce::AudioBuffer<float> buf (1, 600);
            for (int i = 0; i < 600; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 599), 1.0f, 0.01f);

            for (int i = 0; i < 600; ++i)
                buf.setSample (0, i, 0.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 240), 0.5f, 0.03f);
            expectWithinAbsoluteError (buf.getSample (0, 590), 0.0f, 0.01f);

            juce::AudioBuffer<float> retrig (1, 780);
            for (int i = 0; i < 780; ++i)
                retrig.setSample (0, i, (i < 480 || i >= 580) ? 1.0f : 0.0f);
            chain.processBlock (retrig);
            expect (retrig.getSample (0, 600) < 0.1f,
                    "rising gate during release must restart the attack");
            expect (retrig.getSample (0, 700) > 0.1f);
        }

        beginTest ("adsr retrigger off still starts a new note after release");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "adsr1: attack = 0.01; decay = 0.001; sustain = 1; release = 0.01; retrigger = off; gate = audio\n"
                "stage1: y = adsr1\n", err), err);
            chain.prepare ({ 48000.0, 700, 1 });
            juce::AudioBuffer<float> buf (1, 700);
            for (int i = 0; i < 600; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 599), 1.0f, 0.01f);

            for (int i = 0; i < 600; ++i)
                buf.setSample (0, i, 0.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 590), 0.0f, 0.01f);

            for (int i = 0; i < 200; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expect (buf.getSample (0, 120) > 0.1f,
                    "a rising gate after release must start a new attack");
        }

        beginTest ("adsr gate change inside one buffer starts release");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "adsr1: attack = 0.01; decay = 0.01; sustain = 1; release = 0.01; gate = audio\n"
                "stage1: y = adsr1\n", err), err);
            chain.prepare ({ 48000.0, 200, 1 });
            juce::AudioBuffer<float> buf (1, 200);
            for (int i = 0; i < 200; ++i)
                buf.setSample (0, i, i < 100 ? 1.0f : 0.0f);
            chain.processBlock (buf);
            const float atGateEnd = buf.getSample (0, 99);
            expect (atGateEnd > 0.1f);
            expect (buf.getSample (0, 199) < atGateEnd * 0.9f);
        }

        beginTest ("adsr midi gate and velocity drive the lane");
        {
            MidiVariableMapper midi;
            midi.processEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 64));
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "adsr1: attack = 0.001; decay = 0.001; sustain = 1; release = 0.01; gate = midi; velocity = 1\n"
                "stage1: y = adsr1\n", err), err);
            chain.prepare ({ 48000.0, 512, 1 });
            chain.setMidiVariables (midi);
            juce::AudioBuffer<float> buf (1, 512);
            for (int i = 0; i < 512; ++i)
                buf.setSample (0, i, 0.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 500), 64.0f / 127.0f, 0.02f);
        }

        beginTest ("adsr output does not depend on host buffer size");
        {
            auto render = [] (int blocks, int blockSize)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    "adsr1: attack = 0.1; decay = 0.1; sustain = 1; release = 0.1; gate = audio\n"
                    "stage1: y = adsr1\n", err);
                chain.prepare ({ 48000.0, (juce::uint32) blockSize, 1 });
                juce::AudioBuffer<float> buf (1, blockSize);
                float last = 0.f;
                for (int b = 0; b < blocks; ++b)
                {
                    for (int i = 0; i < blockSize; ++i)
                        buf.setSample (0, i, 1.0f);
                    chain.processBlock (buf);
                    last = buf.getSample (0, blockSize - 1);
                }
                return last;
            };
            const float one = render (1, 1000);
            const float many = render (4, 250);
            expectWithinAbsoluteError (one, many, 1.0e-4f);
        }

        beginTest ("samplehold keeps each sampled value for its full interval");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "samplehold1: rate = 100; sync = off; hold = 0; slew = 0; quantize = 0; mix = 1\n"
                "stage1: y = samplehold1\n", err), err);
            chain.prepare ({ 48000.0, 1500, 1 });
            juce::AudioBuffer<float> buf (1, 1500);
            for (int i = 0; i < 1500; ++i)
                buf.setSample (0, i, (float) i / 48000.0f);
            chain.processBlock (buf);
            const float first = buf.getSample (0, 500);
            expectWithinAbsoluteError (first, 480.0f / 48000.0f, 1.0e-4f);
            expectWithinAbsoluteError (buf.getSample (0, 900), first, 1.0e-4f);
            expect (buf.getSample (0, 970) > first * 1.5f,
                    "next sample must land one interval later");
        }

        beginTest ("samplehold rate 0 freezes and reset clears the hold");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "samplehold1: rate = 0; mix = 1\n"
                "stage1: y = samplehold1\n", err), err);
            chain.prepare ({ 48000.0, 1000, 1 });
            juce::AudioBuffer<float> buf (1, 1000);
            for (int i = 0; i < 1000; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 500), 0.0f, 1.0e-6f);

            dsl::SignalChain held;
            expect (held.loadScript (
                "samplehold1: rate = 100; mix = 1\n"
                "stage1: y = samplehold1\n", err), err);
            held.prepare ({ 48000.0, 1000, 1 });
            for (int i = 0; i < 1000; ++i)
                buf.setSample (0, i, 1.0f);
            held.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 900), 1.0f, 1.0e-4f);
            held.clearRuntimeState();
            for (int i = 0; i < 1000; ++i)
                buf.setSample (0, i, 0.0f);
            held.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 900), 0.0f, 1.0e-6f);
        }

        beginTest ("samplehold sync follows host BPM");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "samplehold1: rate = 10; sync = 1/16; mix = 1\n"
                "stage1: y = samplehold1\n", err), err);
            chain.prepare ({ 48000.0, 7000, 1 });
            chain.setTempo (120.0, 0.0, true);
            juce::AudioBuffer<float> buf (1, 7000);
            for (int i = 0; i < 7000; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 5990), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (buf.getSample (0, 6005), 1.0f, 1.0e-4f);
        }

        beginTest ("samplehold quantize and slew shape the held value");
        {
            auto render = [] (const char* extra, float input, int samples)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    juce::String ("samplehold1: rate = 100; sync = off; hold = 0; mix = 1; ")
                    + extra + "\nstage1: y = samplehold1\n", err);
                chain.prepare ({ 48000.0, (juce::uint32) samples, 1 });
                juce::AudioBuffer<float> buf (1, samples);
                for (int i = 0; i < samples; ++i)
                    buf.setSample (0, i, input);
                chain.processBlock (buf);
                return buf.getSample (0, samples - 1);
            };
            expectWithinAbsoluteError (render ("quantize = 0; slew = 0", 0.6f, 600), 0.6f, 1.0e-4f);
            expectWithinAbsoluteError (render ("quantize = 1; slew = 0", 0.6f, 600), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (render ("quantize = 1; slew = 0", 1.0f, 600), 1.0f, 1.0e-4f);
            const float slewed500 = render ("quantize = 0; slew = 0.1", 1.0f, 500);
            const float slewed4800 = render ("quantize = 0; slew = 0.1", 1.0f, 4800);
            expect (slewed500 < 0.5f);
            expect (slewed4800 > 0.8f);
        }

        beginTest ("samplehold output does not depend on host buffer size");
        {
            auto render = [] (int blocks, int blockSize)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    "samplehold1: rate = 100; mix = 1\n"
                    "stage1: y = samplehold1\n", err);
                chain.prepare ({ 48000.0, (juce::uint32) blockSize, 1 });
                juce::AudioBuffer<float> buf (1, blockSize);
                int n = 0;
                float last = 0.f;
                for (int b = 0; b < blocks; ++b)
                {
                    for (int i = 0; i < blockSize; ++i, ++n)
                        buf.setSample (0, i, (float) n / 48000.0f);
                    chain.processBlock (buf);
                    last = buf.getSample (0, blockSize - 1);
                }
                return last;
            };
            expectWithinAbsoluteError (render (1, 2000), render (4, 500), 1.0e-5f);
        }

        beginTest ("random is reproducible per seed and independent between seeds");
        {
            auto render = [] (int seed, int samples)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    "random1: rate = 100; min = 0; max = 1; shape = uniform; smooth = 0; bipolar = off; seed = "
                    + juce::String (seed) + "\nstage1: y = random1\n", err);
                chain.prepare ({ 48000.0, (juce::uint32) samples, 1 });
                juce::AudioBuffer<float> buf (1, samples);
                buf.clear();
                chain.processBlock (buf);
                std::vector<float> out ((size_t) samples);
                for (int i = 0; i < samples; ++i)
                    out[(size_t) i] = buf.getSample (0, i);
                return out;
            };
            const auto a = render (42, 2000);
            const auto b = render (42, 2000);
            const auto c = render (43, 2000);
            for (int i = 500; i < 1800; i += 260)
                expectWithinAbsoluteError (a[(size_t) i], b[(size_t) i], 1.0e-6f);
            bool differs = false;
            for (int i = 500; i < 1800; ++i)
                if (std::abs (a[(size_t) i] - c[(size_t) i]) > 1.0e-5f)
                {
                    differs = true;
                    break;
                }
            expect (differs, "different seeds must produce a different sequence");
        }

        beginTest ("random respects range, swaps min/max and bipolar mode");
        {
            auto render = [] (const char* extra, int samples, std::vector<float>& out)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (juce::String ("random1: rate = 100; ") + extra + "\nstage1: y = random1\n", err);
                chain.prepare ({ 48000.0, (juce::uint32) samples, 1 });
                juce::AudioBuffer<float> buf (1, samples);
                buf.clear();
                chain.processBlock (buf);
                out.resize ((size_t) samples);
                for (int i = 0; i < samples; ++i)
                    out[(size_t) i] = buf.getSample (0, i);
            };
            std::vector<float> bounded;
            render ("min = -0.5; max = 0.5; shape = gaussian; smooth = 0; bipolar = off; mix = 1", 4000, bounded);
            float lo = 1.f, hi = -1.f;
            for (float v : bounded) { lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
            expect (lo >= -0.5f - 1.0e-5f && hi <= 0.5f + 1.0e-5f);

            std::vector<float> swapped;
            render ("min = 1; max = 0; smooth = 0; bipolar = off; mix = 1", 4000, swapped);
            float slo = 1.f, shi = -1.f;
            for (float v : swapped) { slo = juce::jmin (slo, v); shi = juce::jmax (shi, v); }
            expect (slo >= -1.0e-5f && shi <= 1.0f + 1.0e-5f);

            std::vector<float> bipolar;
            render ("min = 0.2; max = 0.8; smooth = 0; bipolar = on; mix = 1", 4000, bipolar);
            float bmin = 1.f, bmax = -1.f;
            for (float v : bipolar) { bmin = juce::jmin (bmin, v); bmax = juce::jmax (bmax, v); }
            expect (bmin < 0.f && bmax > 0.f);
        }

        beginTest ("random sync, smoothing and reset");
        {
            dsl::SignalChain sync;
            juce::String err;
            expect (sync.loadScript (
                "random1: rate = 10; sync = 1/16; min = 1; max = 2; smooth = 0; mix = 1\n"
                "stage1: y = random1\n", err), err);
            sync.prepare ({ 48000.0, 7000, 1 });
            sync.setTempo (120.0, 0.0, true);
            juce::AudioBuffer<float> buf (1, 7000);
            buf.clear();
            sync.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 5990), 0.0f, 1.0e-6f);
            expect (buf.getSample (0, 6005) >= 1.0f && buf.getSample (0, 6005) <= 2.0f);

            auto smoothAt = [] (int samples)
            {
                dsl::SignalChain chain;
                juce::String e;
                chain.loadScript (
                    "random1: rate = 100; min = 1; max = 1; smooth = 0.1; mix = 1\n"
                    "stage1: y = random1\n", e);
                chain.prepare ({ 48000.0, (juce::uint32) samples, 1 });
                juce::AudioBuffer<float> b (1, samples);
                b.clear();
                chain.processBlock (b);
                return b.getSample (0, samples - 1);
            };
            expect (smoothAt (500) < 0.5f);
            expect (smoothAt (4800) > 0.8f);

            auto renderSeed = [] (bool reset)
            {
                dsl::SignalChain chain;
                juce::String e;
                chain.loadScript (
                    "random1: rate = 100; min = 0; max = 1; smooth = 0; seed = 42\n"
                    "stage1: y = random1\n", e);
                chain.prepare ({ 48000.0, 2000, 1 });
                juce::AudioBuffer<float> b (1, 2000);
                b.clear();
                chain.processBlock (b);
                if (reset)
                {
                    chain.clearRuntimeState();
                    b.clear();
                    chain.processBlock (b);
                }
                return b.getSample (0, 1500);
            };
            expectWithinAbsoluteError (renderSeed (false), renderSeed (true), 1.0e-6f);
        }

        beginTest ("envtrigger fires once per transient");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "envtrigger1: thresholddb = -24; sensitivity = 0; hysteresis = 6; holdoff = 0; hold = 50; level = 1; source = peak\n"
                "stage1: y = envtrigger1\n", err), err);
            chain.prepare ({ 48000.0, 10000, 1 });
            juce::AudioBuffer<float> buf (1, 10000);
            buf.clear();
            buf.setSample (0, 100, 1.0f);
            chain.processBlock (buf);
            int edges = buf.getSample (0, 0) > 0.0f ? 1 : 0;
            for (int i = 1; i < 10000; ++i)
                if (buf.getSample (0, i - 1) <= 0.0f && buf.getSample (0, i) > 0.0f)
                    ++edges;
            expectEquals (edges, 1);
            expectWithinAbsoluteError (buf.getSample (0, 200), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (buf.getSample (0, 2600), 0.0f, 1.0e-6f);
        }

        beginTest ("envtrigger holdoff blocks double triggers");
        {
            auto countEdges = [] (float secondAt)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    "envtrigger1: thresholddb = -24; sensitivity = 0; hysteresis = 6; holdoff = 100; hold = 10; level = 1; source = peak\n"
                    "stage1: y = envtrigger1\n", err);
                chain.prepare ({ 48000.0, 10000, 1 });
                juce::AudioBuffer<float> buf (1, 10000);
                buf.clear();
                buf.setSample (0, 100, 1.0f);
                buf.setSample (0, (int) secondAt, 1.0f);
                chain.processBlock (buf);
                int edges = buf.getSample (0, 0) > 0.0f ? 1 : 0;
                for (int i = 1; i < 10000; ++i)
                    if (buf.getSample (0, i - 1) <= 0.0f && buf.getSample (0, i) > 0.0f)
                        ++edges;
                return edges;
            };
            expectEquals (countEdges (200.0f), 1);
            expectEquals (countEdges (6000.0f), 2);
        }

        beginTest ("envtrigger hysteresis requires a real re-arm");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "envtrigger1: thresholddb = -24; sensitivity = 0; hysteresis = 12; holdoff = 0; hold = 10; level = 1; source = peak\n"
                "stage1: y = envtrigger1\n", err), err);
            chain.prepare ({ 48000.0, 5000, 1 });
            juce::AudioBuffer<float> buf (1, 5000);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            for (int i = 1; i < 2000; ++i) buf.setSample (0, i, 0.1f);
            for (int i = 2000; i < 3000; ++i) buf.setSample (0, i, 0.05f);
            for (int i = 3000; i < 3500; ++i) buf.setSample (0, i, 0.001f);
            for (int i = 3500; i < 4000; ++i) buf.setSample (0, i, 0.5f);
            chain.processBlock (buf);
            int edges = buf.getSample (0, 0) > 0.0f ? 1 : 0;
            for (int i = 1; i < 5000; ++i)
                if (buf.getSample (0, i - 1) <= 0.0f && buf.getSample (0, i) > 0.0f)
                    ++edges;
            expectEquals (edges, 2);
        }

        beginTest ("envtrigger threshold edges, silence and rms source");
        {
            auto edgesFor = [] (const char* extra, float level)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    juce::String ("envtrigger1: thresholddb = -24; sensitivity = 0; hysteresis = 6; holdoff = 0; hold = 10; level = 1; ")
                    + extra + "\nstage1: y = envtrigger1\n", err);
                chain.prepare ({ 48000.0, 2000, 1 });
                juce::AudioBuffer<float> buf (1, 2000);
                for (int i = 0; i < 2000; ++i)
                    buf.setSample (0, i, level);
                chain.processBlock (buf);
                int edges = buf.getSample (0, 0) > 0.0f ? 1 : 0;
                for (int i = 1; i < 2000; ++i)
                    if (buf.getSample (0, i - 1) <= 0.0f && buf.getSample (0, i) > 0.0f)
                        ++edges;
                return edges;
            };
            expectEquals (edgesFor ("source = peak", 0.05f), 0);
            expectEquals (edgesFor ("source = peak", 0.0f), 0);
            expectEquals (edgesFor ("source = peak", 0.1f), 1);
            expectEquals (edgesFor ("source = rms", 1.0f), 1);

            dsl::SignalChain legacy;
            juce::String e;
            expect (legacy.loadScript (
                "envtrigger1: threshold = 0.5; hold = 10; level = 1; source = peak\n"
                "stage1: y = envtrigger1\n", e), e);
            legacy.prepare ({ 48000.0, 1000, 1 });
            juce::AudioBuffer<float> b (1, 1000);
            for (int i = 0; i < 1000; ++i)
                b.setSample (0, i, 0.6f);
            legacy.processBlock (b);
            expect (b.getSample (0, 100) > 0.0f, "legacy linear threshold must still fire");
        }

        beginTest ("modmath implements the binary operations on control sources");
        {
            auto value = [] (const char* mode, float a, float b)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    juce::String ("random1: rate = 1000; min = ") + juce::String (a)
                    + "; max = " + juce::String (a)
                    + "\nrandom2: rate = 1000; min = " + juce::String (b)
                    + "; max = " + juce::String (b)
                    + "\nmodmath1: ina = random1; inb = random2; mode = " + mode
                    + "; amount = 1; mix = 1\nstage1: y = modmath1\n", err);
                chain.prepare ({ 48000.0, 2000, 1 });
                juce::AudioBuffer<float> buf (1, 2000);
                buf.clear();
                chain.processBlock (buf);
                return buf.getSample (0, 1500);
            };
            expectWithinAbsoluteError (value ("add", 0.5f, 0.25f), 0.75f, 1.0e-4f);
            expectWithinAbsoluteError (value ("sub", 0.5f, 0.25f), 0.25f, 1.0e-4f);
            expectWithinAbsoluteError (value ("mul", 0.5f, 0.25f), 0.125f, 1.0e-4f);
            expectWithinAbsoluteError (value ("div", 0.25f, 0.5f), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (value ("min", 0.5f, 0.25f), 0.25f, 1.0e-4f);
            expectWithinAbsoluteError (value ("max", 0.5f, 0.25f), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (value ("div", 0.5f, 0.0f), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (value ("abs", -0.5f, 0.0f), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (value ("invert", 0.5f, 0.0f), -0.5f, 1.0e-4f);
        }

        beginTest ("modmath clamp, remap, quantize and slew");
        {
            auto value = [] (const char* mode, float a, const char* extra)
            {
                dsl::SignalChain chain;
                juce::String err;
                chain.loadScript (
                    juce::String ("random1: rate = 1000; min = ") + juce::String (a)
                    + "; max = " + juce::String (a)
                    + "\nmodmath1: ina = random1; mode = " + mode
                    + "; amount = 1; mix = 1; " + extra + "\nstage1: y = modmath1\n", err);
                chain.prepare ({ 48000.0, 2000, 1 });
                juce::AudioBuffer<float> buf (1, 2000);
                buf.clear();
                chain.processBlock (buf);
                return buf.getSample (0, 1500);
            };
            expectWithinAbsoluteError (value ("clamp", 0.5f, "clampmin = 0.2; clampmax = 0.4"), 0.4f, 1.0e-4f);
            expectWithinAbsoluteError (value ("remap", 0.5f, "inmin = 0; inmax = 1; outmin = -1; outmax = 1"), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (value ("quantize", 0.6f, "quantize = 1"), 0.0f, 1.0e-4f);
            const float slewed = value ("slew", 1.0f, "slew = 0.1");
            expect (slewed > 0.0f && slewed < 0.9f);
        }

        beginTest ("modmath renders sources topologically and per sample");
        {
            dsl::SignalChain order;
            juce::String err;
            expect (order.loadScript (
                "modmath1: ina = random1; mode = add; amount = 1; mix = 1\n"
                "random1: rate = 1000; min = 0.5; max = 0.5\n"
                "stage1: y = modmath1\n", err), err);
            order.prepare ({ 48000.0, 1000, 1 });
            juce::AudioBuffer<float> buf (1, 1000);
            buf.clear();
            order.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 500), 0.5f, 1.0e-4f);

            dsl::SignalChain lane;
            expect (lane.loadScript (
                "stepseq1: steps = 2; rate = 100; values = 0.5, -0.5; gate = 1; mix = 1\n"
                "modmath1: ina = stepseq1; mode = add; amount = 1; mix = 1\n"
                "stage1: y = modmath1\n", err), err);
            lane.prepare ({ 48000.0, 1000, 1 });
            juce::AudioBuffer<float> ramp (1, 1000);
            ramp.clear();
            lane.processBlock (ramp);
            expectWithinAbsoluteError (ramp.getSample (0, 100), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (ramp.getSample (0, 600), -0.5f, 1.0e-4f);
        }

        beginTest ("modmix keeps the full source lane");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 2; rate = 100; values = 0.5, -0.5; gate = 1; mix = 1\n"
                "modmix1: ina = stepseq1; levela = 1; levelb = 0; mix = 1\n"
                "stage1: y = modmix1\n", err), err);
            chain.prepare ({ 48000.0, 1000, 1 });
            juce::AudioBuffer<float> buf (1, 1000);
            buf.clear();
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 100), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (buf.getSample (0, 600), -0.5f, 1.0e-4f);
        }

        beginTest ("audio-position sources must precede their consumers");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (! chain.loadScript (
                "modmath1: ina = env1; mode = add; amount = 1; mix = 1\n"
                "env1: type = peak; attack = 0.01; release = 0.1\n"
                "stage1: y = modmath1\n", err));
            expect (err.containsIgnoreCase ("must be declared before"), err);
        }

        beginTest ("one stepseq drives two independent targets");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "stepseq1: steps = 1; rate = 100; values = 0.5\n"
                "stage1: y = x * stepseq1\n"
                "stage2: y = x * stepseq1\n", err), err);
            chain.prepare ({ 48000.0, 256, 1 });
            juce::AudioBuffer<float> buf (1, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 1.0f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 255), 0.25f, 1.0e-3f);
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
