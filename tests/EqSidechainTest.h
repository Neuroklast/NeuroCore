#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "TestHelpers.h"
#include <cmath>

class EqSidechainTest : public juce::UnitTest
{
public:
    EqSidechainTest() : juce::UnitTest ("EqSidechainTest", "DSP") {}

    void runTest() override
    {
        beginTest ("eq block parses peak notch and cut");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse (
                "eq1: type = peak; freq = 1000; q = 1.2; gain = 6\n"
                "eq2: type = notch; freq = 500; q = 8\n"
                "eq3: type = highcut; freq = 8000; q = 0.707\n",
                blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 3);
            expectEquals (blocks[0].type, juce::String ("eq"));
            expectEquals (blocks[1].args.at ("type"), juce::String ("notch"));
            expectEquals (blocks[2].args.at ("type"), juce::String ("highcut"));
        }

        beginTest ("peak at 1 kHz raises a 1 kHz tone more than a 200 Hz tone");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "eq1: type = peak; freq = 1000; q = 2; gain = 12", err), err);
            chain.prepare ({ 48000.0, 512, 2 });

            const float mid = toneRms (chain, 1000.f, 48000.f);
            const float low = toneRms (chain, 200.f, 48000.f);
            expect (mid > low * 1.4f);
        }

        beginTest ("notch at 1 kHz cuts a 1 kHz tone");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "eq1: type = notch; freq = 1000; q = 8; gain = 0", err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            const float mid = toneRms (chain, 1000.f, 48000.f);
            const float low = toneRms (chain, 200.f, 48000.f);
            expect (mid < low * 0.5f);
        }

        beginTest ("stage can read sc from the extra input");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("stage1: y = sc", err), err);
            chain.prepare ({ 48000.0, 256, 2 });

            juce::AudioBuffer<float> sc (2, 256);
            juce::AudioBuffer<float> main (2, 256);
            main.clear();
            for (int i = 0; i < 256; ++i)
            {
                const float s = 0.4f * std::sin (i * 0.1f);
                sc.setSample (0, i, s);
                sc.setSample (1, i, s);
            }
            chain.setExternalSidechain (sc.getReadPointer (0), sc.getReadPointer (1), 256);
            chain.processBlockSmoothed (main, TestHelpers::nullKnobs());
            float peak = 0.f;
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (main.getSample (0, i)));
            expect (peak > 0.2f);
        }

        beginTest ("lfo viz ring holds a sine cycle not a DC slice");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "osc1: shape = sine; freq = 4; depth = 1\n"
                "stage1: y = x * (0.5 + 0.5 * osc1)", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            for (int b = 0; b < 400; ++b)
            {
                buf.clear();
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            }
            float viz[64] {};
            expect (chain.copyLfoViz ("osc1", viz, 64));
            float mn = 1.0e9f, mx = -1.0e9f;
            for (float v : viz)
            {
                mn = juce::jmin (mn, v);
                mx = juce::jmax (mx, v);
            }
            expect (mx - mn > 0.8f, "lfo viz span=" + juce::String (mx - mn, 3));

            float hz = 0.f;
            expect (chain.copyLfoHz ("osc1", hz), "copyLfoHz osc1");
            expect (std::abs (hz - 4.f) < 0.05f, "lfo hz=" + juce::String (hz, 3));
            expect (! chain.copyLfoHz ("stage1", hz));
        }

        beginTest ("copyLfoHz follows freq and stays 0 for missing osc");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "osc1: shape = sine; freq = 1.5; depth = 0.25\n"
                "stage1: y = x * (0.5 + 0.5 * osc1)", err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            buf.clear();
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            float hz = -1.f;
            expect (chain.copyLfoHz ("osc1", hz));
            expect (std::abs (hz - 1.5f) < 0.05f, "lfo hz=" + juce::String (hz, 3));
            float missing = 99.f;
            expect (! chain.copyLfoHz ("osc9", missing));
            expectEquals (missing, 0.f);
        }

        beginTest ("osc shape aliases sawtooth tri pulse stay finite");
        {
            for (const char* shape : { "sawtooth", "tri", "pulse", "ramp" })
            {
                dsl::SignalChain chain;
                juce::String err;
                expect (chain.loadScript (
                    juce::String ("osc1: shape = ") + shape + "; freq = 4; depth = 1\n"
                    "stage1: y = x * (0.5 + 0.5 * osc1)", err), err);
                chain.prepare ({ 48000.0, 256, 2 });
                juce::AudioBuffer<float> buf (2, 256);
                for (int i = 0; i < 256; ++i)
                {
                    const float s = 0.3f * std::sin (2.f * juce::MathConstants<float>::pi * 220.f
                                                    * (float) i / 48000.f);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                expectEquals (TestHelpers::countNonFinite (buf), 0);
            }
        }

        beginTest ("octaver keeps a sine audible");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "octaver1: sub = 0.8; up = 0.15; mix = 0.7; tone = 400; thresh = 0.04",
                err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            const float rms = toneRms (chain, 110.f, 48000.f);
            expect (rms > 0.02f);
        }

        beginTest ("octaver stereo both channels stay finite");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "octaver1: sub = 0.9; up = 0.2; mix = 0.8; tone = 380; thresh = 0.04",
                err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                const float s = 0.35f * std::sin (2.f * juce::MathConstants<float>::pi * 110.f * (float) i / 48000.f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s * 0.85f);
            }
            for (int w = 0; w < 6; ++w)
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (std::abs (buf.getSample (0, 400)) > 1.0e-4f);
            expect (std::abs (buf.getSample (1, 400)) > 1.0e-4f);
        }

        beginTest ("octaver sub on a 110 Hz sine is near 55 Hz and L/R lock");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "octaver1: sub = 1.0; up = 0; mix = 1.0; tone = 280; thresh = 0.04",
                err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            juce::AudioBuffer<float> buf (2, 512);
            const float w = 2.f * juce::MathConstants<float>::pi * 110.f / 48000.f;
            int zx = 0;
            float prev = 0.f;
            bool havePrev = false;
            double corr = 0.0, eL = 0.0, eR = 0.0;
            for (int blk = 0; blk < 16; ++blk)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = 0.4f * std::sin (w * (float) (blk * 512 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s * 0.9f);
                }
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                if (blk < 6)
                    continue;
                for (int i = 0; i < 512; ++i)
                {
                    const float l = buf.getSample (0, i);
                    const float r = buf.getSample (1, i);
                    if (havePrev && prev < 0.f && l >= 0.f)
                        ++zx;
                    prev = l;
                    havePrev = true;
                    corr += (double) l * (double) r;
                    eL += (double) l * (double) l;
                    eR += (double) r * (double) r;
                }
            }
            expect (zx >= 4 && zx <= 16, "sub should sit near 55 Hz, zx=" + juce::String (zx));
            const float den = (float) std::sqrt (eL * eR);
            expect (den > 1.0e-8f && (float) corr / den > 0.92f, "sub must be mono-locked");
            expectEquals (TestHelpers::countNonFinite (buf), 0);
        }

        beginTest ("vocoder self-vocodes without sidechain");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "vocoder1: bands = 4; mix = 0.8; q = 2; formant = 1; dry = 0.2",
                err), err);
            chain.prepare ({ 48000.0, 512, 2 });
            const float rms = toneRms (chain, 440.f, 48000.f);
            expect (rms > 0.01f);
        }

        beginTest ("vocoder follows the extra input");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "vocoder1: bands = 4; mix = 1; q = 2.2; formant = 1; dry = 0",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });

            juce::AudioBuffer<float> sc (2, 256);
            juce::AudioBuffer<float> main (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                const float car = 0.35f * std::sin (i * 0.31f);
                const float mod = 0.5f * std::sin (i * 0.07f);
                main.setSample (0, i, car);
                main.setSample (1, i, car);
                sc.setSample (0, i, mod);
                sc.setSample (1, i, mod);
            }
            for (int w = 0; w < 6; ++w)
            {
                chain.setExternalSidechain (sc.getReadPointer (0), sc.getReadPointer (1), 256);
                chain.processBlockSmoothed (main, TestHelpers::nullKnobs());
                for (int i = 0; i < 256; ++i)
                {
                    const float car = 0.35f * std::sin (i * 0.31f);
                    main.setSample (0, i, car);
                    main.setSample (1, i, car);
                }
            }
            chain.setExternalSidechain (sc.getReadPointer (0), sc.getReadPointer (1), 256);
            chain.processBlockSmoothed (main, TestHelpers::nullKnobs());
            float peak = 0.f;
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (main.getSample (0, i)));
            expect (peak > 0.04f, "sidechain vocoder should imprint, peak="
                    + juce::String (peak, 3));

            juce::AudioBuffer<float> silent (2, 256);
            silent.clear();
            for (int i = 0; i < 256; ++i)
            {
                const float car = 0.35f * std::sin (i * 0.31f);
                main.setSample (0, i, car);
                main.setSample (1, i, car);
            }
            chain.setExternalSidechain (silent.getReadPointer (0), silent.getReadPointer (1), 256);
            chain.processBlockSmoothed (main, TestHelpers::nullKnobs());
            expect (TestHelpers::peakAbs (main) > 0.02f,
                    "empty sidechain must fall back to self-vocode, not mute");
        }

        beginTest ("vocoder voice-jack input acts as modulator");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "vocoder1: bands = 4; mix = 1; q = 2.2; formant = 1; dry = 0",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });

            juce::AudioBuffer<float> voice (2, 256);
            juce::AudioBuffer<float> main (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                const float car = 0.35f * std::sin (i * 0.31f);
                const float mod = 0.5f * std::sin (i * 0.07f);
                main.setSample (0, i, car);
                main.setSample (1, i, car);
                voice.setSample (0, i, mod);
                voice.setSample (1, i, mod);
            }
            for (int w = 0; w < 6; ++w)
            {
                chain.setVoiceInput (voice.getReadPointer (0), voice.getReadPointer (1), 256);
                chain.processBlockSmoothed (main, TestHelpers::nullKnobs());
                for (int i = 0; i < 256; ++i)
                {
                    const float car = 0.35f * std::sin (i * 0.31f);
                    main.setSample (0, i, car);
                    main.setSample (1, i, car);
                }
            }
            chain.setVoiceInput (voice.getReadPointer (0), voice.getReadPointer (1), 256);
            chain.processBlockSmoothed (main, TestHelpers::nullKnobs());
            float peak = 0.f;
            for (int i = 0; i < 256; ++i)
                peak = juce::jmax (peak, std::abs (main.getSample (0, i)));
            expect (peak > 0.04f, "voice-jack vocoder should imprint, peak="
                    + juce::String (peak, 3));
            expect (TestHelpers::countNonFinite (main) == 0, "voice-jack vocoder: no NaN/Inf");
        }

        beginTest ("vocoder attack/release args parse without error");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "vocoder1: bands = 8; mix = 0.8; q = 2; attack = 0.005; release = 0.08",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                buf.setSample (0, i, 0.2f * std::sin (i * 0.25f));
                buf.setSample (1, i, 0.2f * std::sin (i * 0.25f));
            }
            chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
            expect (TestHelpers::countNonFinite (buf) == 0, "attack/release args: no NaN/Inf");
        }

        beginTest ("vocoder kMaxBands = 32 with bands = 32 runs without crash");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "vocoder1: bands = 32; mix = 0.85; q = 2.2; formant = 1; dry = 0.1",
                err), err);
            chain.prepare ({ 48000.0, 256, 2 });
            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                buf.setSample (0, i, 0.2f * std::sin (i * 0.25f));
                buf.setSample (1, i, 0.2f * std::sin (i * 0.25f));
            }
            for (int w = 0; w < 4; ++w)
            {
                chain.processBlockSmoothed (buf, TestHelpers::nullKnobs());
                for (int i = 0; i < 256; ++i)
                {
                    buf.setSample (0, i, 0.2f * std::sin (i * 0.25f));
                    buf.setSample (1, i, 0.2f * std::sin (i * 0.25f));
                }
            }
            expect (TestHelpers::countNonFinite (buf) == 0, "bands=32: no NaN/Inf");
        }
    }

private:
    float toneRms (dsl::SignalChain& chain, float hz, float sr)
    {
        juce::AudioBuffer<float> buf (2, 1024);
        const float w = juce::MathConstants<float>::twoPi * hz / sr;
        for (int i = 0; i < 1024; ++i)
        {
            const float s = 0.25f * std::sin (w * (float) i);
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
        for (int warm = 0; warm < 4; ++warm)
            chain.processBlock (buf);
        for (int i = 0; i < 1024; ++i)
        {
            const float s = 0.25f * std::sin (w * (float) i);
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
        chain.processBlock (buf);
        double acc = 0.0;
        for (int i = 256; i < 1024; ++i)
            acc += (double) buf.getSample (0, i) * (double) buf.getSample (0, i);
        return (float) std::sqrt (acc / 768.0);
    }
};
