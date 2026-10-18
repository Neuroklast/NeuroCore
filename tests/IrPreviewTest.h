#pragma once

#include <JuceHeader.h>
#include "../src/core/PluginProcessor.h"
#include "TestHelpers.h"
#include <atomic>
#include <thread>

class IrPreviewTest : public juce::UnitTest
{
public:
    IrPreviewTest() : juce::UnitTest ("IrPreview", "Persistence") {}

    void runTest() override
    {
        beginTest ("IR preview mixes a snapshot into a silent block");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            prepare (proc);
            expect (loadClick (proc, 64, 0.5f, err), err);
            proc.startIrPreview ("ir1");

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            buf.clear();
            proc.processBlock (buf, midi);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (TestHelpers::peakAbs (buf) > 0.05f, "preview click must be audible");
        }

        beginTest ("IR preview keeps playing after the bank slot is replaced");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            prepare (proc);
            expect (loadClick (proc, 64, 0.5f, err), err);
            proc.startIrPreview ("ir1");
            expect (loadClick (proc, 64, 0.0f, err), err);

            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            buf.clear();
            proc.processBlock (buf, midi);
            expectEquals (TestHelpers::countNonFinite (buf), 0);
            expect (TestHelpers::peakAbs (buf) > 0.05f,
                    "preview must be a snapshot, not the live bank");
        }

        beginTest ("IR preview restart from another thread stays finite");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            prepare (proc);
            expect (loadClick (proc, 64, 0.5f, err), err);

            std::atomic<bool> stop { false };
            std::atomic<int> bad { 0 };
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> buf (2, 64);
                juce::MidiBuffer midi;
                while (! stop.load (std::memory_order_relaxed))
                {
                    buf.clear();
                    proc.processBlock (buf, midi);
                    bad.fetch_add (TestHelpers::countNonFinite (buf),
                                   std::memory_order_relaxed);
                }
            });

            for (int i = 0; i < 80; ++i)
            {
                expect (loadClick (proc, (i & 1) != 0 ? 64 : 2048, 0.5f, err), err);
                proc.startIrPreview ("ir1");
            }

            stop.store (true, std::memory_order_relaxed);
            audio.join();
            expectEquals (bad.load(), 0);
        }
    }

private:
    static void prepare (NeuroKoreAudioProcessor& proc)
    {
        proc.setPlayConfigDetails (2, 2, 44100.0, 64);
        proc.prepareToPlay (44100.0, 64);
        juce::AudioBuffer<float> buf (2, 64);
        juce::MidiBuffer midi;
        for (int b = 0; b < 8; ++b)
        {
            buf.clear();
            proc.processBlock (buf, midi);
        }
    }

    static juce::MemoryBlock makeImpulseWav (int ch, int n, double sr, float first)
    {
        juce::AudioBuffer<float> buf (ch, n);
        buf.clear();
        for (int c = 0; c < ch; ++c)
            if (n > 0)
                buf.setSample (c, 0, first);

        juce::MemoryBlock mb;
        auto* out = new juce::MemoryOutputStream (mb, false);
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> writer (fmt.createWriterFor (
            out, sr, (unsigned) ch, 16, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buf, 0, n);
        writer.reset();
        return mb;
    }

    bool loadClick (NeuroKoreAudioProcessor& proc, int n, float first, juce::String& err)
    {
        const auto wav = makeImpulseWav (1, n, 44100.0, first);
        if (wav.getSize() == 0)
        {
            err = "could not write wav";
            return false;
        }
        return proc.loadIrFromMemory ("ir1", wav.getData(), (int) wav.getSize(),
                                      "click.wav", err);
    }
};
