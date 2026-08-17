#pragma once

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/dsl/DSLParser.h"
#include "TestHelpers.h"
#include <cmath>

class IrXoverTest : public juce::UnitTest
{
public:
    IrXoverTest() : juce::UnitTest ("IrXoverTest", "DSP") {}

    void runTest() override
    {
        beginTest ("xover 2-band splits lows and highs");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "xover1: f1 = 400\nout: low = 1; high = 0", err), err);
            chain.prepare ({ 48000.0, 512, 1 });
            const float low = toneRms (chain, 80.f, 0.5f);
            expect (chain.loadScript (
                "xover1: f1 = 400\nout: low = 0; high = 1", err), err);
            chain.prepare ({ 48000.0, 512, 1 });
            const float high = toneRms (chain, 80.f, 0.5f);
            expect (low > high * 3.f, "80 Hz should live in low, low="
                    + juce::String (low, 4) + " high=" + juce::String (high, 4));
        }

        beginTest ("xover 3-band puts 1 kHz in mid");
        {
            dsl::SignalChain midChain, highChain;
            juce::String err;
            expect (midChain.loadScript (
                "xover1: f1 = 250; f2 = 2500\nout: low = 0; mid = 1; high = 0", err), err);
            midChain.prepare ({ 48000.0, 512, 1 });
            expect (highChain.loadScript (
                "xover1: f1 = 250; f2 = 2500\nout: low = 0; mid = 0; high = 1", err), err);
            highChain.prepare ({ 48000.0, 512, 1 });
            const float mid = toneRms (midChain, 1000.f, 0.5f);
            const float high = toneRms (highChain, 1000.f, 0.5f);
            expect (mid > high * 1.5f, "1 kHz should live in mid, mid="
                    + juce::String (mid, 4) + " high=" + juce::String (high, 4));
            expect (std::isfinite (mid) && std::isfinite (high));
        }

        beginTest ("xover stays finite while the split moves");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "xover1: f1 = 200; f2 = 2500\nout: low = 0.5; mid = 0.35; high = 0.35",
                err), err);
            chain.prepare ({ 48000.0, 64, 2 });
            juce::AudioBuffer<float> buf (2, 64);
            float peak = 0.f;
            for (int block = 0; block < 24; ++block)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float x = (i == 0 && block == 0) ? 1.f
                        : std::sin (6.2831853f * 440.f * (float) (block * 64 + i) / 48000.f);
                    buf.setSample (0, i, x);
                    buf.setSample (1, i, x);
                }
                chain.processBlock (buf);
                for (int i = 0; i < 64; ++i)
                {
                    expect (std::isfinite (buf.getSample (0, i)));
                    peak = juce::jmax (peak, std::abs (buf.getSample (0, i)));
                }
            }
            expect (peak < 8.f, "xover must not explode on an impulse + tone");
        }

        beginTest ("xover split sweep does not zipper");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (
                "xover1: f1 = a * 2000 + 80; f2 = 4000\nout: low = 0.5; mid = 0.35; high = 0.35",
                err), err);
            chain.prepare ({ 48000.0, 64, 1 });
            chain.setParameter (0, 0.1f);
            juce::AudioBuffer<float> buf (1, 64);
            float prev = 0.f;
            float maxJump = 0.f;
            bool havePrev = false;
            for (int block = 0; block < 40; ++block)
            {
                if (block == 8)
                    chain.setParameter (0, 0.8f);
                for (int i = 0; i < 64; ++i)
                    buf.setSample (0, i, 0.35f * std::sin (6.2831853f * 220.f
                        * (float) (block * 64 + i) / 48000.f));
                chain.processBlock (buf);
                for (int i = 0; i < 64; ++i)
                {
                    const float s = buf.getSample (0, i);
                    expect (std::isfinite (s));
                    if (havePrev)
                        maxJump = juce::jmax (maxJump, std::abs (s - prev));
                    prev = s;
                    havePrev = true;
                }
            }
            expect (maxJump < 0.28f, "xover coeff moves must not click, jump="
                    + juce::String (maxJump, 3));
        }

        beginTest ("two ir blocks both parse");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (parser.parse ("ir1: mix = 1\nir2: mix = 0.4", blocks, aliases, params, err), err);
            expectEquals ((int) blocks.size(), 2);
        }

        beginTest ("identity IR is near dry");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("ir1: mix = 1; gain = 0", err), err);
            juce::AudioBuffer<float> ir (1, 8);
            ir.clear();
            ir.setSample (0, 0, 1.f);
            chain.loadImpulseResponse ("ir1", ir, 48000.0);
            chain.prepare ({ 48000.0, 64, 1 });

            juce::AudioBuffer<float> buf (1, 64);
            buf.clear();
            buf.setSample (0, 0, 0.8f);
            chain.processBlock (buf);
            float peak = 0.f;
            int peakAt = 0;
            for (int i = 0; i < 64; ++i)
            {
                const float v = std::abs (buf.getSample (0, i));
                if (v > peak) { peak = v; peakAt = i; }
            }
            expect (peak > 0.5f, "impulse IR should pass energy, peak=" + juce::String (peak, 3));
            expect (peakAt < 16);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
        }

        beginTest ("empty IR is dry");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("ir1: mix = 1", err), err);
            chain.prepare ({ 48000.0, 32, 1 });
            juce::AudioBuffer<float> buf (1, 32);
            for (int i = 0; i < 32; ++i)
                buf.setSample (0, i, 0.25f);
            chain.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 8), 0.25f, 1.0e-4f);
        }
    }

private:
    static float toneRms (dsl::SignalChain& chain, float hz, float amp)
    {
        juce::AudioBuffer<float> buf (1, 512);
        double acc = 0.0;
        int n = 0;
        for (int b = 0; b < 6; ++b)
        {
            for (int i = 0; i < 512; ++i, ++n)
            {
                const float s = amp * std::sin (2.f * juce::MathConstants<float>::pi * hz * (float) n / 48000.f);
                buf.setSample (0, i, s);
            }
            chain.processBlock (buf);
            for (int i = 0; i < 512; ++i)
                acc += (double) buf.getSample (0, i) * (double) buf.getSample (0, i);
        }
        return (float) std::sqrt (acc / (512.0 * 6.0));
    }
};
