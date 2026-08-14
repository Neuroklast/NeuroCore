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
            expect (peak > 0.01f);
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
