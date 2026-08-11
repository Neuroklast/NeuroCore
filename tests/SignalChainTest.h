#ifndef SIGNALCHAINTEST_H
#define SIGNALCHAINTEST_H

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/core/Config.h"

class SignalChainTest : public juce::UnitTest
{
public:
    SignalChainTest() : juce::UnitTest("SignalChainTest", "DSL") {}

    void runTest() override
    {
        beginTest("Parse and process simple chain");
        dsl::SignalChain chain;
        juce::String err;
        juce::String script = "stage1: y = x * a\n";
        expect(chain.loadScript(script, err));
        expect(err.isEmpty());
        juce::dsp::ProcessSpec spec{44100.0, 4, 1};
        chain.prepare(spec);
        juce::AudioBuffer<float> buffer(1,4);
        buffer.clear();
        buffer.setSample(0,0,1.0f);
        chain.setValueTreeState(nullptr);
        // Knobs are 0–1; stage output is hard-limited to [-1, 1]
        chain.setParameter(0, 0.5f);
        chain.processBlock(buffer);
        expectWithinAbsoluteError(buffer.getSample(0,0), 0.5f, 1e-5f);

        beginTest("Filter block with high cutoff");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nfilter1: type = lowpass; cutoff = 20000";
            expect(c.loadScript(sc, e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1,2); buf.clear(); buf.setSample(0,0,1.0f);
            c.setValueTreeState(nullptr);
            c.processBlock(buf);
            expectGreaterOrEqual(buf.getSample(0,0), 0.5f);
        }

        beginTest("Bandpass center/width");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nfilter1: type = bandpass; center = 1000; width = 500";
            expect(c.loadScript(sc, e));
            expect(e.isEmpty());
            c.prepare(spec);
        }

        beginTest("Bandpass low/high cut");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nfilter1: type = bandpass; lowcut = 400; highcut = 1600";
            expect(c.loadScript(sc, e));
            expect(e.isEmpty());
            c.prepare(spec);
        }

        beginTest("Compressor reduces level");
        {
            dsl::SignalChain c;
            juce::String e;
            // JUCE compressor threshold is dB; drive a full-scale signal hard
            juce::String sc = "stage1: y = x\ncomp1: threshold = -24; ratio = 8; attack = 0.001; release = 0.05";
            expect(c.loadScript(sc, e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1, 256);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                buf.setSample(0, i, 1.0f);
            c.setValueTreeState(nullptr);
            for (int n = 0; n < 8; ++n)
                c.processBlock(buf);
            expectLessThan(buf.getSample(0, 255), 0.95f);
        }

        beginTest("Envelope follower pass-through");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nenv1: type = peak";
            expect(c.loadScript(sc, e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1,1); buf.setSample(0,0,0.5f);
            c.setValueTreeState(nullptr);
            c.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0,0), 0.5f, 1e-5f);
        }

        beginTest("Parameter alias usage");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "param a = gain\nstage1: y = gain * x";
            expect(c.loadScript(sc, e));
            expect(e.isEmpty());
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1,1); buf.setSample(0,0,1.0f);
            c.setValueTreeState(nullptr);
            c.setParameter(0, 0.5f);
            c.processBlock(buf);
            expectWithinAbsoluteError(buf.getSample(0,0), 0.5f, 1e-5f);
        }

        beginTest("processBlockSmoothed knob routing");
        {
            dsl::SignalChain c;
            juce::String e;
            expect(c.loadScript("stage1: y = x * a", e));
            c.prepare(spec);

            juce::SmoothedValue<float> aSm;
            aSm.reset(spec.sampleRate, Config::kSmoothingTime);
            aSm.setCurrentAndTargetValue(0.5f);

            juce::AudioBuffer<float> buf(1, 1);
            buf.setSample(0, 0, 1.0f);
            c.processBlockSmoothed(buf, { &aSm, nullptr, nullptr, nullptr });
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.5f, 1e-5f);
        }

        beginTest("Block path for simple stage");
        {
            dsl::SignalChain c;
            juce::String e;
            expect(c.loadScript("stage1: y = tanh(x * a)", e));
            c.prepare(spec);

            juce::SmoothedValue<float> aSm;
            aSm.reset(spec.sampleRate, Config::kSmoothingTime);
            aSm.setCurrentAndTargetValue(2.0f);

            juce::AudioBuffer<float> buf(2, 16);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                buf.setSample(0, i, 0.5f);
                buf.setSample(1, i, 0.25f);
            }

            c.processBlockSmoothed(buf, { &aSm, nullptr, nullptr, nullptr });
            expect(std::isfinite(buf.getSample(0, 15)));
            expect(std::isfinite(buf.getSample(1, 15)));
            expectGreaterThan(std::abs(buf.getSample(0, 15)), 0.4f);
        }

        beginTest("Filter parameter smoothing");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nfilter1: cutoff = map(a,0,1,500,5000)";
            expect(c.loadScript(sc, e));
            expect(e.isEmpty());
            c.prepare(spec);

            juce::SmoothedValue<float> aSm;
            aSm.reset(spec.sampleRate, Config::kSmoothingTime);
            aSm.setCurrentAndTargetValue(0.f);
            c.setValueTreeState(nullptr);
            juce::AudioBuffer<float> buf(1,8); buf.clear();
            juce::AudioBuffer<float> tmp(1,1); tmp.setSample(0,0,1.0f);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                if (i % 2 == 0)
                    aSm.setTargetValue(1.f);
                else
                    aSm.setTargetValue(0.f);
                tmp.setSample(0,0,1.0f);
                c.processBlock(tmp);
                buf.setSample(0,i, tmp.getSample(0,0));
                expect(std::isfinite(buf.getSample(0,i)));
            }
        }
    }
};

#endif // SIGNALCHAINTEST_H
