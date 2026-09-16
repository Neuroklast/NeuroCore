#pragma once
#include "../src/dsl/SignalChain.h"
#include "../src/dsp/InputRouter.h"
#include "TestHelpers.h"
#include "../src/core/CpuProtect.h"

#if NK_ALLOCATION_PROBE
extern "C" void nkBeginAllocationProbe();
extern "C" unsigned nkEndAllocationProbe();
#endif
// Shared by the real-time contract test and the opt-in benchmark runner.
namespace FxRuntime
{
struct Case { const char* name; const char* script; };
inline constexpr Case cases[] {
    { "bus gain", "param a = Blend [0.2, 0.8]\nstage1: y = x\nbus wet:\nsend: in = a * 0.55\nstage2: y = x\nout: main = 1 - a; wet = a" },
    { "parallel", "stage1: y = x\nbus wet:\nsend: in = 1\npitch1: semitones = 0; mix = 1\nout: main = 0.5; wet = 0.5" },
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
#if NK_ALLOCATION_PROBE
        beginTest ("prepared production DSP callbacks do not allocate heap memory");
        for (const auto& fx : FxRuntime::cases)
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript (fx.script, error), error);
            chain.prepare ({ 48000, 257, 2 });
            juce::AudioBuffer<float> b (2, 257);
            unsigned allocations = 0;
            for (int block = 0; block < 16; ++block)
            {
                FxRuntime::fill (b, block * 257);
                chain.setParameter (0, (float) block / 15.f);
                nkBeginAllocationProbe();
                chain.processBlock (b);
                allocations += nkEndAllocationProbe();
            }
            expectEquals ((int) allocations, 0, fx.name);
        }
#endif
        beginTest ("triangle LFO has a continuous turnaround, not a saw reset");
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("osc1: shape = triangle; freq = 20\nstage1: y = osc1 * 0.2", error), error);
            chain.prepare ({ 48000, 256, 1 });
            juce::AudioBuffer<float> b (1, 256);
            float previous = 0, maxStep = 0;
            for (int block = 0; block < 50; ++block)
            {
                b.clear(); chain.processBlock (b);
                for (int i = 0; i < 256; ++i)
                {
                    const float x = b.getSample (0, i);
                    if (block > 10) maxStep = juce::jmax (maxStep, std::abs (x - previous));
                    previous = x;
                }
            }
            expect (maxStep < 0.001f, "triangle step=" + juce::String (maxStep, 6));
        }
        beginTest ("stage MS encoding is applied exactly once");
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("stage1: y = x; ms_encode = true", error), error);
            chain.prepare ({ 48000, 31, 2 });
            juce::AudioBuffer<float> b (2, 31);
            for (int i = 0; i < 31; ++i) { b.setSample (0, i, 0.3f); b.setSample (1, i, 0.1f); }
            chain.processBlock (b);
            expectWithinAbsoluteError (b.getSample (0, 15), 0.2f, 1.e-6f);
            expectWithinAbsoluteError (b.getSample (1, 15), 0.1f, 1.e-6f);
        }
        beginTest ("three-band crossover recombines without a magnitude hole");
        for (float hz : { 200.f, 800.f, 1200.f, 2500.f, 8000.f })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("xover1: f1 = 1000; f2 = 1500\nout: low = 1; mid = 1; high = 1", error), error);
            chain.prepare ({ 48000, 256, 1 });
            juce::AudioBuffer<float> b (1, 256);
            double input = 0, output = 0;
            for (int block = 0; block < 64; ++block)
            {
                for (int i = 0; i < 256; ++i)
                {
                    float x = 0.1f * std::sin (juce::MathConstants<float>::twoPi * hz * (block * 256 + i) / 48000.f);
                    b.setSample (0, i, x);
                    if (block >= 32) input += x*x;
                }
                chain.processBlock (b);
                if (block >= 32)
                    for (int i = 0; i < 256; ++i) output += b.getSample (0, i) * b.getSample (0, i);
            }
            expect (std::abs (10.0 * std::log10 (output/input)) < 0.1, "Hz=" + juce::String (hz) + " dB=" + juce::String (10.0 * std::log10 (output/input)));
        }
        beginTest ("envelope bounds follow automation and absent external sidechain is silent");
        for (const auto* script : { "env1: type = peak; min = a; max = 1\nstage1: y = env1",
                                    "env1: type = peak; source = sidechain; attack = 0.0001\nstage1: y = env1" })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript (script, error), error);
            chain.prepare ({ 48000, 256, 1 });
            chain.setParameter (0, 0.5f);
            juce::AudioBuffer<float> b (1, 256);
            const bool external = juce::String (script).contains ("sidechain");
            for (int block = 0; block < 10; ++block)
            {
                for (int i = 0; i < 256; ++i) b.setSample (0, i, external ? 0.2f : 0.f);
                chain.processBlock (b);
            }
            expectWithinAbsoluteError (b.getSample (0, 255), external ? 0.f : 0.5f, 0.001f);
        }
        beginTest ("limiter reprepare recomputes rate-dependent release");
        {
            dsl::SignalChain reused, fresh;
            juce::String error;
            const juce::String script = "limit1: ceiling = -12; release = 0.1";
            expect (reused.loadScript (script, error) && fresh.loadScript (script, error), error);
            reused.prepare ({ 22050, 256, 1 });
            juce::AudioBuffer<float> a (1, 256), b (1, 256);
            FxRuntime::fill (a); reused.processBlock (a);
            reused.prepare ({ 192000, 256, 1 }); fresh.prepare ({ 192000, 256, 1 });
            float maxError = 0;
            for (int block = 0; block < 20; ++block)
            {
                for (int i = 0; i < 256; ++i) a.setSample (0, i, block < 2 ? 0.8f : 0.1f);
                b.makeCopyOf (a, true); reused.processBlock (a); fresh.processBlock (b);
                for (int i = 0; i < 256; ++i) maxError = juce::jmax (maxError, std::abs (a.getSample (0,i) - b.getSample (0,i)));
            }
            expect (maxError < 1.e-6f, "reprepare error=" + juce::String (maxError));
        }
        beginTest ("phaser and flanger mix automation does not step at callback boundaries");
        for (const auto* script : { "phaser1: rate = 0; depth = 0; feedback = 0.5; mix = a",
                                    "flanger1: rate = 0; depth = 0; feedback = 0.5; delay = 2; mix = a" })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript (script, error), error);
            chain.prepare ({ 48000, 64, 1 });
            chain.setParameter (0, 0.f);
            juce::AudioBuffer<float> b (1, 64);
            for (int block = 0; block < 150; ++block)
            {
                for (int i = 0; i < 64; ++i) b.setSample (0, i, 0.2f);
                chain.processBlock (b);
            }
            const float previous = b.getSample (0, 63);
            chain.setParameter (0, 1.f);
            for (int i = 0; i < 64; ++i) b.setSample (0, i, 0.2f);
            chain.processBlock (b);
            expect (std::abs (b.getSample (0, 0) - previous) < 0.001f, juce::String (script));
        }
        beginTest ("offline rendering never trips or holds the CPU watchdog");
        {
            CpuProtect guard;
            guard.reset();
            for (int i = 0; i < 1000; ++i) guard.observe (0.1, 0.01, true);
            expect (! guard.isTripped(), "offline work has no realtime deadline");
            for (int i = 0; i < 1000; ++i) guard.observe (0.1, 0.01);
            expect (guard.isTripped(), "same load must still protect realtime audio");
            expect (! guard.shouldHoldAudio (64, 48000, true), "offline render must run wet even after a live trip");
        }
        beginTest ("parallel pitch buses and dry sends align at the longest audible path");
        for (const auto* script : {
            "stage1: y = x\nbus shifted:\nsend: in = 1\npitch1: semitones = 0; formant = 1; mix = 1\nout: main = 0.5; shifted = 0.5",
            "pitch1: semitones = 0; formant = 1; mix = 1\nbus shifted:\nsend: in = 1\npitch2: semitones = 0; formant = 1; mix = 1\nout: main = 0.5; shifted = 0.5",
            "pitch1: semitones = 0; formant = 1; mix = 1\nbus joined:\nsend: main = 0.5; in = 0.5\nstage1: y = x\nout: joined = 1" })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript (script, error), error);
            chain.prepare ({ 48000, 256, 2 });
            expectEquals (chain.getIrLatencySamples(), 1024);
            juce::AudioBuffer<float> b (2, 256);
            double difference = 0, energy = 0;
            for (int block = 0; block < 48; ++block)
            {
                FxRuntime::fill (b, block * 256); chain.processBlock (b);
                if (block < 16) continue;
                for (int i = 0; i < 256; ++i)
                {
                    const float expected = 0.15f * std::sin ((block * 256 + i - 1024) * 0.057f);
                    const float delta = b.getSample (0, i) - expected;
                    difference += delta * delta; energy += expected * expected;
                }
            }
            expect (difference / energy < 0.002, "parallel relative error=" + juce::String (difference / energy));
        }
        beginTest ("neutral OTT bands keep unit magnitude at every depth");
        for (float depth : { 0.f, 0.5f, 1.f })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("ott1: f1 = 1000; f2 = 1500; low = 0; mid = 0; high = 0; depth = " + juce::String (depth), error), error);
            chain.prepare ({ 48000, 256, 1 });
            juce::AudioBuffer<float> b (1, 256);
            double energy = 0;
            for (int block = 0; block < 64; ++block)
            {
                for (int i = 0; i < 256; ++i)
                    b.setSample (0, i, 0.1f * std::sin (juce::MathConstants<float>::twoPi * 800.f * (block * 256 + i) / 48000.f));
                chain.processBlock (b);
                if (block >= 32) for (int i = 0; i < 256; ++i) energy += b.getSample (0,i) * b.getSample (0,i);
            }
            const double db = 10.0 * std::log10 (energy / (32 * 256 * 0.005));
            expect (std::abs (db) < 0.1, "depth=" + juce::String (depth) + " dB=" + juce::String (db));
        }
        beginTest ("bus gains evaluate compound expressions in declared parameter ranges");
        for (const auto& gain : { "a * 0.55", "Blend * 0.55", "1 - (a * 0.5)" })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("param a = Blend [0.2, 0.8]\nstage1: y = x\nbus wet:\nsend: in = "
                + juce::String (gain) + "\nstage2: y = x\nout: main = 0; wet = 1", error), error);
            chain.prepare ({ 48000, 64, 1 });
            chain.setParameter (0, 0.25f); // physical value 0.35
            juce::AudioBuffer<float> b (1, 64);
            for (int block = 0; block < 32; ++block)
            {
                for (int j = 0; j < 64; ++j) b.setSample (0, j, 0.2f);
                chain.processBlock (b);
            }
            const float expected = juce::String (gain).startsWith ("1") ? 0.825f : 0.1925f;
            expectWithinAbsoluteError (b.getSample (0, 63), 0.2f * expected, 0.0001f);
        }
        beginTest ("invalid bus control variables fail at load");
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (! chain.loadScript ("stage1: y = x\nout: main = unknown * 0.5", error));
            expect (error.contains ("Unknown bus gain variable"));
        }
        beginTest ("unity stage preserves full-scale samples within internal headroom");
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("stage1: y = x", error), error);
            chain.prepare ({ 48000, 64, 1 });
            juce::AudioBuffer<float> b (1, 64);
            for (int i = 0; i < 64; ++i) b.setSample (0, i, i % 2 ? 1.f : -1.f);
            chain.processBlock (b);
            expectWithinAbsoluteError (b.getSample (0, 0), -1.f, 1.e-6f);
            expectWithinAbsoluteError (b.getSample (0, 1), 1.f, 1.e-6f);
        }
        beginTest ("pitch analysis duration stays constant at oversampled rates");
        for (int rate : { 48000, 96000, 192000, 384000 })
        {
            dsl::SignalChain chain;
            juce::String error;
            expect (chain.loadScript ("pitch1: semitones = 0; mix = 1", error), error);
            chain.prepare ({ (double) rate, 256, 1 });
            expectEquals (chain.getIrLatencySamples(), 1024 * (rate / 48000));
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
