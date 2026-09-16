#pragma once
#include "../src/dsl/SignalChain.h"
#include "../src/dsp/InputRouter.h"
#include "TestHelpers.h"

// Shared by the real-time contract test and the opt-in benchmark runner.
namespace FxRuntime
{
struct Case { const char* name; const char* script; };
inline constexpr Case cases[] {
    { "stage", "stage1: y = tanh(x * (1 + a))" },
    { "osc", "osc1: shape = triangle; freq = 2\nstage1: y = x * (0.7 + osc1 * 0.2)" },
    { "env", "env1: type = peak; attack = 0.001 + a * 0.01; release = 0.1\nstage1: y = x * (1 - env1 * 0.3)" },
    { "filter", "filter1: type = lowpass; cutoff = 800 + a * 4000; resonance = 0.7" },
    { "eq", "eq1: type = peak; freq = 800 + a * 4000; q = 0.7; gain = 3" },
    { "gate", "gate1: threshold = -40 + a * 10; attack = 0.001; release = 0.08" },
    { "comp", "comp1: threshold = -20 + a * 10; ratio = 4; attack = 0.01; release = 0.1" },
    { "limit", "limit1: ceiling = -6 + a * 3; release = 0.08" },
    { "delay", "delay1: time = 0.01 + a * 0.05; feedback = 0.3; mix = 0.25" },
    { "reverb", "reverb1: size = 0.2 + a * 0.5; damp = 0.4; mix = 0.25" },
    { "ott", "ott1: depth = 0.3; time = 0.35; f1 = 90 + a * 100; f2 = 3200" },
    { "widen", "widen1: width = a; delay = 14; bass = 140" },
    { "phaser", "phaser1: rate = 0.4; depth = 0.7; center = 800; mix = a" },
    { "flanger", "flanger1: rate = 0.25; depth = 0.7; delay = 2; mix = a" },
    { "octaver", "octaver1: sub = a; up = 0.2; mix = 0.8" },
    { "pitch", "pitch1: semitones = a * 12; formant = 1; mix = 0.5" },
    { "vocoder", "vocoder1: bands = 12; q = 2.2; formant = 0.8 + a; mix = 0.5" },
    { "ir", "ir1: mix = a; gain = 0" },
    { "sidechain", "sidechain1: mix = a" },
    { "meter", "meter1: type = rms" },
    { "ms", "ms1: mode = encode\nstage1: y = x\nms2: mode = decode" },
    { "xover", "xover1: f1 = 100 + a * 200; f2 = 3000\nout: low = 1; mid = 1; high = 1" }
};
inline void fill (juce::AudioBuffer<float>& b, int offset = 0)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (c, i, 0.15f * std::sin ((float) (i + offset) * (c == 0 ? 0.057f : 0.081f)));
}
}

class FxRuntimeTest : public juce::UnitTest
{
public:
    FxRuntimeTest() : UnitTest ("FxRuntimeTest", "DSP") {}
    void runTest() override
    {
        beginTest ("every node processes mono/stereo, automation and irregular host blocks");
        for (const auto& fx : FxRuntime::cases)
        {
            logMessage (fx.name);
            for (double sr : { 22050.0, 48000.0, 192000.0 })
                for (int channels : { 1, 2 })
                {
                    dsl::SignalChain chain;
                    juce::String err;
                    if (! chain.loadScript (fx.script, err)) { expect (false, juce::String (fx.name) + ": " + err); continue; }
                    chain.prepare ({ sr, 512, (juce::uint32) channels });
                    int bad = 0;
                    float peak = 0;
                    for (int n : { 0, 1, 7, 31, 64, 257, 512 })
                    {
                        juce::AudioBuffer<float> b (channels, n);
                        FxRuntime::fill (b);
                        chain.setParameter (0, 0.8f);
                        chain.processBlock (b);
                        bad += TestHelpers::countNonFinite (b);
                        peak = juce::jmax (peak, TestHelpers::peakAbs (b));
                    }
                    expect (bad == 0 && peak <= 2.f, juce::String (fx.name) + " sr=" + juce::String (sr) + " ch=" + juce::String (channels));
                }
        }
        beginTest ("input routing preserves a hard-panned stereo source");
        {
            InputRouter router;
            router.prepare ({ 48000, 256, 2 });
            juce::AudioBuffer<float> b (2, 256);
            for (int block = 0; block < 10; ++block)
            {
                b.clear();
                for (int i = 0; i < 256; ++i) b.setSample (0, i, 0.2f);
                router.processBlock (b);
            }
            expect (b.getMagnitude (1, 0, 256) == 0.f, "BOTH must not auto-fill the silent stereo channel");
        }
        beginTest ("envelope measures the signal at its position in the chain");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("stage1: y = x * 0.1\nenv1: type = peak; attack = 0.0001; release = 0.0001\nstage2: y = env1", err), err);
            chain.prepare ({ 48000, 256, 2 });
            juce::AudioBuffer<float> b (2, 256);
            for (int block = 0; block < 4; ++block)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i) b.setSample (c, i, 0.5f);
                chain.processBlock (b);
            }
            expectWithinAbsoluteError (b.getSample (0, 255), 0.05f, 0.001f);
        }
        beginTest ("filter automation advances with one-sample callbacks");
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("filter1: type = lowpass; cutoff = 100 + a * 10000; resonance = 0.7", err), err);
            chain.prepare ({ 48000, 1, 1 });
            chain.setParameter (0, 1.f);
            juce::AudioBuffer<float> b (1, 1);
            float peak = 0;
            for (int i = 0; i < 12000; ++i)
            {
                b.setSample (0, 0, 0.2f * std::sin (0.26f * i)); chain.processBlock (b);
                if (i > 10000) peak = juce::jmax (peak, std::abs (b.getSample (0,0)));
            }
            expect (peak > 0.1f, "one-sample callbacks must not freeze cutoff at 100 Hz");
        }
        beginTest ("width zero preserves stereo and mono widening preserves mono");
        for (int channels : { 1, 2 })
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript (channels == 1 ? "widen1: width = 1" : "widen1: width = 0", err), err);
            chain.prepare ({ 48000, 256, (juce::uint32) channels });
            juce::AudioBuffer<float> b (channels, 256), reference (channels, 256);
            FxRuntime::fill (b); reference.makeCopyOf (b);
            chain.processBlock (b);
            float error = 0;
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < 256; ++i)
                    error = juce::jmax (error, std::abs (b.getSample (c, i) - reference.getSample (c, i)));
            expect (error < 1.e-6f, "width passthrough error=" + juce::String (error));
        }
        beginTest ("pitch unity preserves level and aligns dry and wet at the reported latency");
        for (float mix : { 0.f, 0.5f, 1.f })
        {
            dsl::SignalChain chain;
            juce::String err;
            expect (chain.loadScript ("pitch1: semitones = 0; formant = 1; mix = " + juce::String (mix), err), err);
            chain.prepare ({ 48000, 256, 2 });
            juce::AudioBuffer<float> b (2, 256);
            double energy = 0, difference = 0;
            const int latency = chain.getIrLatencySamples();
            for (int block = 0; block < 48; ++block)
            {
                FxRuntime::fill (b, block * 256);
                chain.processBlock (b);
                if (block < 16) continue;
                for (int i = 0; i < 256; ++i)
                {
                    const float reference = 0.15f * std::sin ((block * 256 + i - latency) * 0.057f);
                    energy += reference * reference;
                    const float delta = b.getSample (0, i) - reference;
                    difference += delta * delta;
                }
            }
            expect (difference / energy < 0.002, "mix=" + juce::String (mix) + " relative error=" + juce::String (difference / energy, 6));
        }
    }
};
