#pragma once

#include <JuceHeader.h>
#include "../src/core/Config.h"
#include "../src/dsp/DcBlocker1p.h"
#include "../src/dsp/AntiAliasLowpass.h"
#include "../src/dsp/TruePeakLimiter.h"
#include "../src/dsp/TpdfDither.h"
#include "../src/dsp/SanitationChain.h"
#include "../src/dsp/DSPUtils.h"
#include "TestHelpers.h"
#include <cmath>

class SanitationChainTest : public juce::UnitTest
{
public:
    SanitationChainTest() : juce::UnitTest ("SanitationChainTest", "DSP") {}

    void runTest() override
    {
        const float ceilLin = juce::Decibels::decibelsToGain (Config::kSanitationCeilingDbTp);

        beginTest ("DC 1-pole removes offset and leaves 1 kHz");
        {
            DcBlocker1p dc;
            dc.prepare (48000.0, 1);
            juce::AudioBuffer<float> buf (1, 48000);
            for (int i = 0; i < 48000; ++i)
            {
                const float s = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 1000.0f * (float) i / 48000.0f);
                buf.setSample (0, i, 0.25f + s);
            }
            auto block = juce::dsp::AudioBlock<float> (buf);
            dc.process (block);
            const int skip = 8000;
            const double mean = DSPUtils::detectDCOffset (
                buf.getReadPointer (0) + skip, (size_t) (48000 - skip));
            expect (std::abs (mean) < 1.0e-3, "DC residual=" + juce::String (mean, 6));
            float pk = 0.f;
            for (int i = skip; i < 48000; ++i)
                pk = juce::jmax (pk, std::abs (buf.getSample (0, i)));
            expect (pk > 0.48f && pk < 0.53f, "1 kHz peak=" + juce::String (pk, 4));
        }

        beginTest ("AA cutoff is below host Nyquist");
        {
            AntiAliasLowpass aa;
            aa.prepare (48000.0 * 8, 1024 * 8, 1, 48000.0, false);
            expect (aa.cutoffHz() < 24000.0);
            expect (aa.cutoffHz() > 20000.0);
        }

        beginTest ("AA slope meets 96 dB/oct (Live 16th) at 2*fc vs passband");
        {
            AntiAliasLowpass aa;
            const double hostSr = 48000.0;
            const double osSr = hostSr * 8.0;
            aa.prepare (osSr, 8192, 1, hostSr, true);
            const double fc = aa.cutoffHz();
            auto tonePeak = [&] (double hz) -> float
            {
                AntiAliasLowpass f;
                f.prepare (osSr, 8192, 1, hostSr, true);
                const int n = 32768;
                juce::AudioBuffer<float> buf (1, n);
                for (int i = 0; i < n; ++i)
                    buf.setSample (0, i, std::sin (2.0 * juce::MathConstants<double>::pi
                                                   * hz * (double) i / osSr));
                auto block = juce::dsp::AudioBlock<float> (buf);
                f.process (block);
                float pk = 0.f;
                for (int i = n / 2; i < n; ++i)
                    pk = juce::jmax (pk, std::abs (buf.getSample (0, i)));
                return pk;
            };
            const float pass = tonePeak (fc * 0.5);
            const float stop = tonePeak (fc * 2.0);
            expect (pass > 0.7f, "passband peak=" + juce::String (pass, 4));
            expect (stop < pass * 3.2e-5f + 1.0e-4f,
                    "stop/pass=" + juce::String (stop / juce::jmax (pass, 1.0e-12f), 8));
        }

        beginTest ("soft clip rounds |x|>1 and can be skipped");
        {
            expect (std::abs (DSPUtils::sanitationSoftClip (4.0f)) < 1.0f);
            expectWithinAbsoluteError (DSPUtils::sanitationSoftClip (0.0f), 0.0f, 1.0e-6f);
        }

        beginTest ("brickwall sample peak never exceeds ceiling");
        {
            TruePeakLimiter lim;
            lim.prepare ({ 48000.0, 512, 2 }, 1);
            juce::AudioBuffer<float> buf (2, 512);
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    buf.setSample (0, i, 1.5f);
                    buf.setSample (1, i, -1.8f);
                }
                auto block = juce::dsp::AudioBlock<float> (buf);
                lim.process (block);
            }
            expect (TestHelpers::peakAbs (buf) <= ceilLin + 1.0e-4f);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
        }

        beginTest ("True Peak of a reconstructed tone stays <= -0.3 dBTP");
        {
            TruePeakLimiter lim;
            lim.prepare ({ 48000.0, 512, 1 }, 1);
            juce::AudioBuffer<float> buf (1, 512);
            auto tpPeak = [] (const juce::AudioBuffer<float>& b) -> float
            {
                float tp = 0.f;
                const int n = b.getNumSamples();
                for (int i = 1; i < n - 2; ++i)
                {
                    const float p0 = b.getSample (0, i - 1);
                    const float p1 = b.getSample (0, i);
                    const float p2 = b.getSample (0, i + 1);
                    const float p3 = b.getSample (0, i + 2);
                    tp = juce::jmax (tp, std::abs (p1));
                    for (int k = 1; k < 4; ++k)
                    {
                        const float t = (float) k / 4.f;
                        const float t2 = t * t, t3 = t2 * t;
                        const float y = 0.5f * ((2.f * p1) + (-p0 + p2) * t
                            + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2
                            + (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
                        tp = juce::jmax (tp, std::abs (y));
                    }
                }
                return tp;
            };
            for (int b = 0; b < 12; ++b)
            {
                for (int i = 0; i < 512; ++i)
                    buf.setSample (0, i, 0.99f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                            * (float) i * 0.25f));
                auto block = juce::dsp::AudioBlock<float> (buf);
                lim.process (block);
            }
            expect (tpPeak (buf) <= ceilLin + 2.0e-3f,
                    "TP=" + juce::String (tpPeak (buf), 6) + " ceil=" + juce::String (ceilLin, 6));
        }

        beginTest ("dither is identity on float (bits=0)");
        {
            TpdfDither d;
            d.prepare (1);
            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i)
                buf.setSample (0, i, 0.1f * (float) i / 64.f);
            juce::AudioBuffer<float> orig;
            orig.makeCopyOf (buf);
            auto block = juce::dsp::AudioBlock<float> (buf);
            d.process (block, 0);
            float maxDiff = 0.f;
            for (int i = 0; i < 64; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (0, i) - orig.getSample (0, i)));
            expect (maxDiff < 1.0e-12f);
        }

        beginTest ("dither engages on 16-bit and stays DC-free");
        {
            TpdfDither d;
            d.prepare (1);
            juce::AudioBuffer<float> buf (1, 4096);
            for (int i = 0; i < 4096; ++i)
                buf.setSample (0, i, 0.001f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                         * (float) i / 64.f));
            juce::AudioBuffer<float> orig;
            orig.makeCopyOf (buf);
            auto block = juce::dsp::AudioBlock<float> (buf);
            d.process (block, 16);
            float maxDiff = 0.f;
            double acc = 0.0;
            for (int i = 0; i < 4096; ++i)
            {
                maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (0, i) - orig.getSample (0, i)));
                acc += (double) buf.getSample (0, i);
            }
            expect (maxDiff > 1.0e-7f);
            expect (std::abs (acc / 4096.0) < 2.0e-4);
        }

        beginTest ("dither does not run when bits>=32");
        {
            TpdfDither d;
            d.prepare (1);
            juce::AudioBuffer<float> buf (1, 32);
            for (int i = 0; i < 32; ++i)
                buf.setSample (0, i, 0.3f);
            auto block = juce::dsp::AudioBlock<float> (buf);
            d.process (block, 32);
            expectWithinAbsoluteError (buf.getSample (0, 0), 0.3f, 1.0e-12f);
        }

        beginTest ("processOversampled does not change block length");
        {
            SanitationChain chain;
            juce::dsp::ProcessSpec host { 48000.0, 256, 1 };
            chain.prepare (host, host, true);
            juce::AudioBuffer<float> buf (1, 256);
            buf.clear();
            auto block = juce::dsp::AudioBlock<float> (buf);
            chain.processOversampled (block);
            expectEquals ((int) block.getNumSamples(), 256);
        }

        beginTest ("soft clip is skippable; limiter is not");
        {
            SanitationChain chain;
            juce::dsp::ProcessSpec host { 48000.0, 256, 1 };
            chain.prepare (host, host, true);
            chain.setSoftClipEnabled (false);
            juce::AudioBuffer<float> dry (1, 256), wet (1, 256);
            for (int b = 0; b < 8; ++b)
            {
                for (int i = 0; i < 256; ++i)
                {
                    dry.setSample (0, i, 0.2f);
                    wet.setSample (0, i, 2.0f);
                }
                auto d = juce::dsp::AudioBlock<const float> (dry);
                auto w = juce::dsp::AudioBlock<float> (wet);
                chain.processHost (d, w);
            }
            expect (TestHelpers::peakAbs (wet) <= ceilLin + 1.0e-4f);
        }

        beginTest ("bypass host path delays without clipping");
        {
            SanitationChain chain;
            juce::dsp::ProcessSpec host { 48000.0, 256, 1 };
            chain.prepare (host, host, true);
            chain.setSoftClipEnabled (true);
            juce::AudioBuffer<float> wet (1, 256);
            wet.clear();
            wet.setSample (0, 0, 1.5f);
            auto w = juce::dsp::AudioBlock<float> (wet);
            chain.processHostBypass (w);
            const float pk = TestHelpers::peakAbs (wet);
            expect (pk > 1.1f, "bypass must not true-peak clip, peak=" + juce::String (pk, 4));
            expectEquals (TestHelpers::countNonFinite (wet), 0);
        }

        beginTest ("1x chain: DC gone, peak <= ceiling, float dither off");
        {
            SanitationChain chain;
            juce::dsp::ProcessSpec host { 48000.0, 512, 1 };
            chain.prepare (host, host, true);
            chain.setSoftClipEnabled (false);
            chain.setIntegerOutputBits (0);
            juce::AudioBuffer<float> buf (1, 512);
            juce::AudioBuffer<float> dry (1, 512);
            for (int b = 0; b < 16; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = 1.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                     * 200.0f * (float) (b * 512 + i) / 48000.0f);
                    buf.setSample (0, i, s + 0.2f);
                    dry.setSample (0, i, 0.2f);
                }
                auto os = juce::dsp::AudioBlock<float> (buf);
                chain.processOversampled (os);
                auto d = juce::dsp::AudioBlock<const float> (dry);
                chain.processHost (d, os);
            }
            const double mean = DSPUtils::detectDCOffset (
                buf.getReadPointer (0) + Config::kSanitationLookaheadHost,
                (size_t) (512 - Config::kSanitationLookaheadHost));
            expect (std::abs (mean) < 0.08, "DC after chain=" + juce::String (mean, 5));
            expect (TestHelpers::peakAbs (buf) <= ceilLin + 1.0e-4f);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
        }
    }
};
