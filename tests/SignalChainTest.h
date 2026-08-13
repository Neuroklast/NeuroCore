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
        // Knobs are 0â€“1; stage output is hard-limited to [-1, 1]
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
            c.processBlockSmoothed(buf, { &aSm, nullptr, nullptr, nullptr, nullptr, nullptr });
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.5f, 1e-5f);
        }

        beginTest("Multi-stage y=f(y) is not silent");
        {
            // Regression: stages like "y = tube(y, â€¦)" used to read y=0 â†’ silence
            dsl::SignalChain c;
            juce::String e;
            expect(c.loadScript(
                "stage1: y = x * 0.5\n"
                "stage2: y = y * 2.0\n"
                "stage3: y = y * 0.5", e), e);
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1, 4);
            buf.setSample(0, 0, 1.0f);
            c.setValueTreeState(nullptr);
            c.processBlock(buf);
            // 1 * 0.5 * 2 * 0.5 = 0.5
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.5f, 1e-4f);
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

            c.processBlockSmoothed(buf, { &aSm, nullptr, nullptr, nullptr, nullptr, nullptr });
            expect(std::isfinite(buf.getSample(0, 15)));
            expect(std::isfinite(buf.getSample(1, 15)));
            expectGreaterThan(std::abs(buf.getSample(0, 15)), 0.4f);
        }

        beginTest("y_prev feedback stays on block path (serial stage only)");
        {
            // Regression: y_prev used to force sampleÃ—channelÃ—block processing of
            // the entire chain (filters included) â†’ catastrophic CPU.
            // Now processBlockSmoothed always uses block path; Stage runs scalar.
            dsl::SignalChain c;
            juce::String e;
            expect(c.loadScript(
                "filter1: type = lowpass; cutoff = 2000; resonance = 0.5\n"
                "stage1: y = softclip(x + y_prev * 0.4, 1.2)\n"
                "filter2: type = highpass; cutoff = 80; resonance = 0.3", e), e);
            c.prepare(spec);
            expect(dsl::SignalChain::canUseBlockPath(*c.getChain()));

            juce::SmoothedValue<float> aSm;
            aSm.reset(spec.sampleRate, Config::kSmoothingTime);
            aSm.setCurrentAndTargetValue(0.5f);

            juce::AudioBuffer<float> buf(2, 256);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float s = 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi
                                                 * 440.0f * (float) i / (float) spec.sampleRate);
                buf.setSample(0, i, s);
                buf.setSample(1, i, s * 0.8f);
            }

            c.processBlockSmoothed(buf, { &aSm, nullptr, nullptr, nullptr, nullptr, nullptr });
            float peak = 0.f;
            int bad = 0;
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float l = buf.getSample(0, i), r = buf.getSample(1, i);
                if (! std::isfinite(l) || ! std::isfinite(r)) ++bad;
                peak = juce::jmax(peak, std::abs(l));
            }
            expectEquals(bad, 0);
            expectGreaterThan(peak, 0.05f);
        }

        beginTest("multi-stage: only one y_prev stage, pre/post pure (factory topology)");
        {
            // Mirrors Preamp Regen Stack: tube (SIMD) -> one y_prev stage -> softclip + LPF
            dsl::SignalChain c;
            juce::String e;
            expect(c.loadScript(
                "param a = Drive [1.2, 7.0]\n"
                "param b = Regen [0.08, 0.48]\n"
                "filter1: type = highpass; cutoff = 70; resonance = 0.35\n"
                "stage1: y = tube(x, a * 0.55)\n"
                "stage2: y = tube(x + y_prev * b, a * 0.4)\n"
                "filter2: type = lowpass; cutoff = 3200; resonance = 0.45\n"
                "stage3: y = softclip(y, 1.15)", e), e);
            c.prepare(spec);
            // Hybrid path always reports block-capable (filters not dragged into sample path)
            expect(dsl::SignalChain::canUseBlockPath(*c.getChain()));

            juce::SmoothedValue<float> aSm, bSm;
            aSm.reset(spec.sampleRate, Config::kSmoothingTime);
            bSm.reset(spec.sampleRate, Config::kSmoothingTime);
            aSm.setCurrentAndTargetValue(0.55f);
            bSm.setCurrentAndTargetValue(0.4f);

            juce::AudioBuffer<float> buf(2, 512);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float s = 0.35f * std::sin(2.0f * juce::MathConstants<float>::pi
                                                  * 220.0f * (float) i / (float) spec.sampleRate);
                buf.setSample(0, i, s);
                buf.setSample(1, i, s);
            }
            // Warm-up + process several blocks (feedback must stay finite)
            for (int n = 0; n < 8; ++n)
                c.processBlockSmoothed(buf, { &aSm, &bSm, nullptr, nullptr, nullptr, nullptr });

            float peak = 0.f;
            int bad = 0;
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float v = buf.getSample(0, i);
                if (! std::isfinite(v)) ++bad;
                peak = juce::jmax(peak, std::abs(v));
            }
            expectEquals(bad, 0);
            expectGreaterThan(peak, 0.02f);
            expectLessThan(peak, 2.5f);
        }

        beginTest("x_prev one-sample delay is correct on block path");
        {
            dsl::SignalChain c;
            juce::String e;
            expect(c.loadScript("stage1: y = x_prev", e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1, 4);
            buf.setSample(0, 0, 1.0f);
            buf.setSample(0, 1, 0.5f);
            buf.setSample(0, 2, 0.25f);
            buf.setSample(0, 3, 0.125f);
            juce::SmoothedValue<float> aSm;
            aSm.reset(spec.sampleRate, Config::kSmoothingTime);
            aSm.setCurrentAndTargetValue(0.f);
            c.processBlockSmoothed(buf, { &aSm, nullptr, nullptr, nullptr, nullptr, nullptr });
            // First sample: prev=0; then delayed input (with tiny leak factor)
            expectWithinAbsoluteError(buf.getSample(0, 0), 0.0f, 1e-4f);
            expectWithinAbsoluteError(buf.getSample(0, 1), 1.0f * Config::kFeedbackLeakFactor, 1e-3f);
            expectWithinAbsoluteError(buf.getSample(0, 2), 0.5f * Config::kFeedbackLeakFactor, 1e-3f);
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

        beginTest("serial passthrough y=x");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript ("stage1: y = x", e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 4);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            c.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 0), 1.0f, 1.0e-4f);
        }

        beginTest("parallel dirt mixes with main");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "stage1: y = x\n"
                "bus dirt:\n"
                "send: in = 1\n"
                "stage2: y = x * 0.5\n"
                "out: main = 1; dirt = 1\n", e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 4);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            c.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 0), 1.5f, 1.0e-4f);
        }

        beginTest("send from processed main");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "stage1: y = x * 0.5\n"
                "bus aux:\n"
                "send: main = 1\n"
                "stage2: y = x\n"
                "out: main = 0; aux = 1\n", e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 4);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            c.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 0), 0.5f, 1.0e-4f);
        }

        beginTest("no out ignores named bus");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "stage1: y = x * 0.25\n"
                "bus dirt:\n"
                "send: in = 1\n"
                "stage2: y = 0\n", e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 4);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            c.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 0), 0.25f, 1.0e-4f);
        }

        beginTest("complementary out 1-c crossfade");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "param c = Blend [0, 1]\n"
                "stage1: y = x\n"
                "bus dirt:\n"
                "send: in = 1\n"
                "stage2: y = 0\n"
                "out: main = 1-c; dirt = c\n", e), e);
            c.prepare (spec);
            c.setParameter (2, 0.5f); // c
            juce::AudioBuffer<float> buf (1, 4);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            c.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 0), 0.5f, 1.0e-4f);
        }

        beginTest("band split sum");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "bus low:\n"
                "send: in = 1\n"
                "stage1: y = x * 0.5\n"
                "bus high:\n"
                "send: in = 1\n"
                "stage2: y = x * 0.5\n"
                "out: low = 1; high = 1\n", e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 4);
            buf.clear();
            buf.setSample (0, 0, 1.0f);
            c.processBlock (buf);
            expectWithinAbsoluteError (buf.getSample (0, 0), 1.0f, 1.0e-4f);
        }
    }
};

#endif // SIGNALCHAINTEST_H
