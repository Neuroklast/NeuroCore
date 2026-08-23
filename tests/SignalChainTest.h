#ifndef SIGNALCHAINTEST_H
#define SIGNALCHAINTEST_H

#include <JuceHeader.h>
#include <cmath>
#include "../src/dsl/SignalChain.h"
#include "../src/dsp/InputRouter.h"
#include "../src/core/Config.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "TestHelpers.h"

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

        beginTest("tap peak reports clip after a hot stage");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript ("stage1: y = x * 4\n", e), e);
            juce::dsp::ProcessSpec spec { 48000.0, 64, 1 };
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i)
                buf.setSample (0, i, 0.6f);
            c.processBlock (buf);
            float pk = 0.f;
            expect (c.copyTapPeak ("stage1", pk));
            expect (pk >= 0.99f);
            float pkL = 0.f, pkR = 0.f;
            expect (c.copyTapPeakLR ("stage1", pkL, pkR));
            expect (std::abs (pkL - pkR) < 0.05f);
        }

        beginTest("stereo tap keeps L and R isolated");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript ("stage1: y = x\n", e), e);
            juce::dsp::ProcessSpec spec2 { 48000.0, 64, 2 };
            c.prepare (spec2);
            juce::AudioBuffer<float> buf (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 0.8f);
                buf.setSample (1, i, 0.0f);
            }
            c.processBlock (buf);
            float pkL = 0.f, pkR = 0.f;
            expect (c.copyTapPeakLR ("stage1", pkL, pkR));
            expect (pkL > 0.5f, "left tap silent after hard pan");
            expect (pkR < 0.05f, "right tap should stay quiet");
            juce::Array<juce::var> clips;
            c.appendClipPeaks (clips);
            bool found = false;
            for (const auto& v : clips)
            {
                if (auto* o = v.getDynamicObject())
                {
                    if (o->getProperty ("id").toString() == "stage1")
                    {
                        found = true;
                        expect (o->hasProperty ("peakL"));
                        expect (o->hasProperty ("peakR"));
                        expect (o->hasProperty ("rmsL"));
                        expect (o->hasProperty ("rmsR"));
                        expect ((float) o->getProperty ("peakL") > 0.5f);
                        expect ((float) o->getProperty ("peakR") < 0.05f);
                        expect ((float) o->getProperty ("rmsL") > 0.5f);
                        expect ((float) o->getProperty ("rmsR") < 0.05f);
                    }
                }
            }
            expect (found);
        }

        beginTest("clip tap rms is block energy, not a copy of peak");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript ("stage1: y = x\n", e), e);
            juce::dsp::ProcessSpec specSine { 48000.0, 64, 2 };
            c.prepare (specSine);
            juce::AudioBuffer<float> buf (2, 64);
            const float twoPi = juce::MathConstants<float>::twoPi;
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 0.8f * std::sin (twoPi * (float) i / 64.f));
                buf.setSample (1, i, 0.0f);
            }
            c.processBlock (buf);
            juce::Array<juce::var> clips;
            c.appendClipPeaks (clips);
            bool found = false;
            for (const auto& v : clips)
            {
                if (auto* o = v.getDynamicObject())
                {
                    if (o->getProperty ("id").toString() == "stage1")
                    {
                        found = true;
                        const float peakL = (float) o->getProperty ("peakL");
                        const float rmsL = (float) o->getProperty ("rmsL");
                        const float rmsR = (float) o->getProperty ("rmsR");
                        expect (peakL > 0.7f, "sine peak missing");
                        expect (rmsL > 0.4f && rmsL < peakL - 0.05f, "rms must sit below peak on a sine");
                        expect (rmsR < 0.05f, "right rms should stay quiet");
                    }
                }
            }
            expect (found);
        }

        beginTest("stage filter limit processBlock not per-sample virtual");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "stage1: y = x\nfilter1: type = lowpass; cutoff = 18000\nlimit1: ceiling = -0.3; release = 0.05",
                e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 0.4f);
                buf.setSample (1, i, 0.4f);
            }
            c.processBlock (buf);
            const float pk = std::max (std::abs (buf.getSample (0, 32)), std::abs (buf.getSample (1, 32)));
            expect (pk > 0.2f && pk < 0.5f, "peak=" + juce::String (pk, 3));
            expect (TestHelpers::countNonFinite (buf) == 0);
        }

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

        beginTest("hardclip-only stage stays finite without ADAA sample inject");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript ("stage1: y = hardclip(x, 0.5)", e), e);
            c.prepare (spec);
            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i)
                buf.setSample (0, i, 1.2f);
            c.processBlock (buf);
            for (int i = 0; i < 64; ++i)
                expect (std::isfinite (buf.getSample (0, i)));
        }

        beginTest("gabber-like env clip + buses stay finite");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "env1: type = peak; attack = 0.001; release = 0.1\n"
                "stage1: y = hardclip(softclip(x * (0.76 + 0.24 * env1), 1.14), 0.7)\n"
                "bus scream:\n"
                "send: in = 1\n"
                "stage2: y = tube(x * (0.12 + 0.88 * env1), 1.4)\n"
                "out: main = 0.58; scream = 0.95\n", e), e);
            juce::dsp::ProcessSpec os { 192000.0, 512, 2 };
            c.prepare (os);
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                const float s = 0.8f * std::sin (2.f * juce::MathConstants<float>::pi * 60.f * (float) i / 192000.f);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            for (int b = 0; b < 8; ++b)
                c.processBlock (buf);
            expect (std::isfinite (buf.getSample (0, 255)));
            expect (std::isfinite (buf.getSample (1, 255)));
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

        beginTest("mono guitar in feeds both L and R channel paths");
        {
            InputRouter router;
            router.prepare ({ 48000.0, 64, 2 });
            router.setUseLeft (true);
            router.setUseRight (true);
            juce::AudioBuffer<float> warm (2, 64);
            warm.clear();
            for (int b = 0; b < 32; ++b)
                router.processBlock (warm);

            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "stage1: channel = left; y = x * 0.5\n"
                "stage2: channel = right; y = x * 0.8\n", e), e);
            c.prepare ({ 48000.0, 64, 2 });
            juce::AudioBuffer<float> buf (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                buf.setSample (0, i, 0.4f);
                buf.setSample (1, i, 0.f);
            }
            router.processBlock (buf);
            c.processBlock (buf);
            float lPeak = 0.f, rPeak = 0.f;
            for (int i = 0; i < 64; ++i)
            {
                lPeak = juce::jmax (lPeak, std::abs (buf.getSample (0, i)));
                rPeak = juce::jmax (rPeak, std::abs (buf.getSample (1, i)));
            }
            expect (lPeak > 0.1f, "left path should hear the mono DI");
            expect (rPeak > 0.1f, "right path should hear the copied mono DI");
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

        beginTest ("env follower maps silence to min and a hot peak toward max");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "env1: type = peak; attack = 0.001; release = 0.05; min = 0.2; max = 0.8\n"
                "stage1: y = env1\n", e), e);
            const juce::dsp::ProcessSpec envSpec { 44100.0, 256, 2 };
            c.prepare (envSpec);

            // Host callbacks get fresh input. Reusing the stage output (y = env1)
            // makes the follower track its own CV and settle at 0.5.
            juce::AudioBuffer<float> silentIn (2, 256);
            silentIn.clear();
            float quiet = 0.f;
            for (int b = 0; b < 8; ++b)
            {
                juce::AudioBuffer<float> block (silentIn);
                c.processBlock (block);
                quiet = block.getSample (0, 255);
            }
            expect (quiet >= 0.15f && quiet <= 0.28f, "silence maps near min " + juce::String (quiet, 4));

            juce::AudioBuffer<float> hotIn (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                hotIn.setSample (0, i, 1.0f);
                hotIn.setSample (1, i, 1.0f);
            }
            float loud = 0.f;
            for (int b = 0; b < 16; ++b)
            {
                juce::AudioBuffer<float> block (hotIn);
                c.processBlock (block);
                loud = block.getSample (0, 255);
            }
            expect (loud >= 0.7f && loud <= 0.85f, "peak maps near max " + juce::String (loud, 4));
        }

        beginTest ("env invert swaps min and max");
        {
            dsl::SignalChain c;
            juce::String e;
            expect (c.loadScript (
                "env1: type = peak; attack = 0.001; release = 0.05; min = 0.1; max = 0.9; invert = on\n"
                "stage1: y = env1\n", e), e);
            const juce::dsp::ProcessSpec envSpec { 44100.0, 256, 2 };
            c.prepare (envSpec);
            juce::AudioBuffer<float> hotIn (2, 256);
            for (int i = 0; i < 256; ++i)
            {
                hotIn.setSample (0, i, 1.0f);
                hotIn.setSample (1, i, 1.0f);
            }
            float y = 0.f;
            for (int b = 0; b < 16; ++b)
            {
                juce::AudioBuffer<float> block (hotIn);
                c.processBlock (block);
                y = block.getSample (0, 255);
            }
            expect (y >= 0.05f && y <= 0.25f, "inverted peak near min " + juce::String (y, 4));
        }

        beginTest ("Kick Rumble env1 tap follows a kick, not silence");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            const auto* row = lib.findByName ("Kick Rumble");
            expect (row != nullptr, "missing Kick Rumble");
            if (row == nullptr)
                return;
            dsl::SignalChain c;
            juce::String err;
            expect (c.loadScript (row->script, err), err);
            const juce::dsp::ProcessSpec kickSpec { 48000.0, 64, 2 };
            c.prepare (kickSpec);
            juce::AudioBuffer<float> buf (2, 64);
            float peak = 0.f;
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const float t = (float) (b * 64 + i) / 48000.f;
                    const float s = std::exp (-t / 0.04f)
                        * std::sin (2.f * juce::MathConstants<float>::pi * 55.f * t);
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                c.processBlock (buf);
                float wave[16] {};
                expect (c.copyNodeTap ("env1", wave, 16), "env1 must publish a tap like osc");
                for (int i = 0; i < 16; ++i)
                    peak = juce::jmax (peak, std::abs (wave[i]));
            }
            expect (peak > 0.15f, "env1 stayed silent " + juce::String (peak, 4));
        }
    }
};

#endif // SIGNALCHAINTEST_H
