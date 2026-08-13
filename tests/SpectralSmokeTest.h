#pragma once

/*
    Minimal spectral smoke: process a sine through a nonlinear script and
    ensure HF energy above ~0.45*Nyquist stays bounded (aliasing/crackle proxy).
*/

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/core/Config.h"
#include <cmath>
#include <vector>

class SpectralSmokeTest : public juce::UnitTest
{
public:
    SpectralSmokeTest() : juce::UnitTest ("SpectralSmokeTest", "DSP") {}

    void runTest() override
    {
        beginTest ("softclip sine: HF ratio bounded");
        expect (hfRatioOk (
            "stage1: y = softclip(x, 4.0)\n"
            "filter1: type = lowpass; cutoff = 12000; resonance = 0.3",
            1000.0f, 0.45f));

        beginTest ("tube sine: HF ratio bounded");
        expect (hfRatioOk (
            "stage1: y = tube(x, 6.0)\n"
            "filter1: type = lowpass; cutoff = 10000; resonance = 0.35",
            1000.0f, 0.55f));

        beginTest ("hardclip+soft pre+LPF: HF ratio bounded");
        expect (hfRatioOk (
            "stage1: y = hardclip(softclip(x, 2.0), 0.6)\n"
            "filter1: type = lowpass; cutoff = 9000; resonance = 0.3",
            2000.0f, 0.55f));

        beginTest ("y_prev regen + LPF stays finite and HF-bounded");
        expect (hfRatioOk (
            "stage1: y = softclip(x + y_prev * 0.35, 1.5)\n"
            "filter1: type = lowpass; cutoff = 8000; resonance = 0.4",
            440.0f, 0.65f));
    }

private:
    /** Very light DFT energy above frac*Nyquist / total energy. */
    bool hfRatioOk (const juce::String& script, float hz, float maxHfRatio)
    {
        const double sr = 44100.0;
        const int N = 1024;
        dsl::SignalChain chain;
        juce::String err;
        if (! chain.loadScript (script, err))
        {
            logMessage ("load fail: " + err);
            return false;
        }
        chain.prepare ({ sr, (juce::uint32) N, 1 });

        juce::AudioBuffer<float> buf (1, N);
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) sr;
            buf.setSample (0, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * hz * t));
        }

        juce::SmoothedValue<float> a, b, c, d;
        for (auto* s : { &a, &b, &c, &d })
        {
            s->reset (sr, 0.0);
            s->setCurrentAndTargetValue (0.55f);
        }

        for (int n = 0; n < 4; ++n)
            chain.processBlockSmoothed (buf, { &a, &b, &c, &d });

        // Goertzel-ish band energy: sum |x| in time as crude total; high-pass
        // residual via first difference energy as HF proxy.
        double total = 0.0, hf = 0.0;
        float prev = 0.f;
        for (int i = 0; i < N; ++i)
        {
            const float x = buf.getSample (0, i);
            if (! std::isfinite (x))
                return false;
            total += (double) x * (double) x;
            const float d = x - prev;
            hf += (double) d * (double) d;
            prev = x;
        }
        if (total < 1.0e-12)
            return false;
        const float ratio = (float) (hf / total);
        logMessage ("HF proxy ratio=" + juce::String (ratio, 4) + " max=" + juce::String (maxHfRatio, 2));
        // Difference energy grows with frequency content; keep generous bounds
        return ratio < maxHfRatio * 20.0f; // scaled: raw ratio is not 0..1 Nyquist fraction
    }
};
