#pragma once

#include <JuceHeader.h>
#include "../src/core/Config.h"
#include "../src/core/PluginProcessor.h"
#include <cmath>
#include <cstring>

class IrStateBoundsTest : public juce::UnitTest
{
public:
    IrStateBoundsTest() : juce::UnitTest ("IrStateBounds", "Persistence") {}

    void runTest() override
    {
        beginTest ("IR state roundtrip restores samples and rate");
        {
            NeuroKoreAudioProcessor src;
            juce::String err;
            expect (src.setFormula ("stage1: y = x * a", err), err);
            expect (loadClick (src, 64, err), err);
            expectEquals (src.getIrNumSamples ("ir1"), 64);
            expectEquals (src.getIrNumChannels ("ir1"), 1);
            expectWithinAbsoluteError (src.getIrSampleRate ("ir1"), 44100.0, 0.1);
            const float first = src.getIrBuffer ("ir1")->getSample (0, 0);

            juce::MemoryBlock blob;
            src.getStateInformation (blob);
            NeuroKoreAudioProcessor dst;
            dst.setStateInformation (blob.getData(), (int) blob.getSize());
            expectEquals (dst.getScript(), juce::String ("stage1: y = x * a"));
            expectEquals (dst.getIrNumSamples ("ir1"), 64);
            expectEquals (dst.getIrNumChannels ("ir1"), 1);
            expectWithinAbsoluteError (dst.getIrSampleRate ("ir1"), 44100.0, 0.1);
            expect (dst.getIrBuffer ("ir1") != nullptr);
            expectWithinAbsoluteError (dst.getIrBuffer ("ir1")->getSample (0, 0), first, 1.0e-3f);
        }

        beginTest ("oversized IR sample count is rejected and previous IR remains");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("stage1: y = x * a", err), err);
            expect (loadClick (proc, 64, err), err);
            const auto blob = patchedIrState (proc, [] (juce::ValueTree ir)
            {
                const double sr = (double) ir.getProperty ("sr", 44100.0);
                const int ch = juce::jmax (1, (int) ir.getProperty ("ch", 1));
                const int n = (int) std::lround (sr * (double) Config::kIrMaxSeconds) + 1;
                ir.setProperty ("n", n, nullptr);
                juce::MemoryBlock raw ((size_t) ch * (size_t) n * sizeof (float));
                raw.fillWith (0);
                ir.setProperty ("data", juce::var (raw), nullptr);
                ir.getParent().setProperty ("DSLScript", "filter1: cutoff = 800", nullptr);
            });
            proc.setStateInformation (blob.getData(), (int) blob.getSize());
            expectEquals (proc.getScript(), juce::String ("stage1: y = x * a"));
            expectEquals (proc.getIrNumSamples ("ir1"), 64);
            expectEquals (proc.getIrName ("ir1"), juce::String ("click.wav"));
        }

        beginTest ("IR channel count above two is rejected and previous IR remains");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("stage1: y = x * a", err), err);
            expect (loadClick (proc, 64, err), err);
            const auto blob = patchedIrState (proc, [] (juce::ValueTree ir)
            {
                const int n = 64;
                const int ch = 8;
                ir.setProperty ("ch", ch, nullptr);
                ir.setProperty ("n", n, nullptr);
                juce::MemoryBlock raw ((size_t) ch * (size_t) n * sizeof (float));
                raw.fillWith (0);
                ir.setProperty ("data", juce::var (raw), nullptr);
            });
            proc.setStateInformation (blob.getData(), (int) blob.getSize());
            expectEquals (proc.getIrNumChannels ("ir1"), 1);
            expectEquals (proc.getIrNumSamples ("ir1"), 64);
        }

        beginTest ("IR payload size mismatch is rejected and previous IR remains");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (loadClick (proc, 64, err), err);
            const float first = proc.getIrBuffer ("ir1")->getSample (0, 0);
            const auto blob = patchedIrState (proc, [] (juce::ValueTree ir)
            {
                ir.setProperty ("n", 64, nullptr);
                juce::MemoryBlock raw (16);
                raw.fillWith (0);
                ir.setProperty ("data", juce::var (raw), nullptr);
            });
            proc.setStateInformation (blob.getData(), (int) blob.getSize());
            expectEquals (proc.getIrNumSamples ("ir1"), 64);
            expectWithinAbsoluteError (proc.getIrBuffer ("ir1")->getSample (0, 0), first, 1.0e-3f);
        }

        beginTest ("out-of-range IR sample rate is rejected and previous IR remains");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (loadClick (proc, 64, err), err);
            const auto blob = patchedIrState (proc, [] (juce::ValueTree ir)
            {
                ir.setProperty ("sr", 0.0, nullptr);
            });
            proc.setStateInformation (blob.getData(), (int) blob.getSize());
            expectWithinAbsoluteError (proc.getIrSampleRate ("ir1"), 44100.0, 0.1);
            expectEquals (proc.getIrNumSamples ("ir1"), 64);
        }

        beginTest ("file import truncates IRs longer than kIrMaxSeconds");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            const double sr = 44100.0;
            const int longN = (int) std::lround (sr * ((double) Config::kIrMaxSeconds + 1.0));
            const auto wav = makeImpulseWav (1, longN, sr);
            expect (wav.getSize() > 0);
            expect (proc.loadIrFromMemory ("ir1", wav.getData(), (int) wav.getSize(),
                                           "long.wav", err), err);
            expectEquals (proc.getIrNumSamples ("ir1"),
                          (int) std::lround (sr * (double) Config::kIrMaxSeconds));
        }
    }

private:
    static juce::MemoryBlock makeImpulseWav (int ch, int n, double sr)
    {
        juce::AudioBuffer<float> buf (ch, n);
        buf.clear();
        for (int c = 0; c < ch; ++c)
            if (n > 0)
                buf.setSample (c, 0, 0.5f);

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

    bool loadClick (NeuroKoreAudioProcessor& proc, int n, juce::String& err)
    {
        const auto wav = makeImpulseWav (1, n, 44100.0);
        if (wav.getSize() == 0)
        {
            err = "could not write wav";
            return false;
        }
        return proc.loadIrFromMemory ("ir1", wav.getData(), (int) wav.getSize(),
                                      "click.wav", err);
    }

    template <typename Fn>
    juce::MemoryBlock patchedIrState (NeuroKoreAudioProcessor& proc, Fn&& mutate) const
    {
        juce::MemoryBlock blob;
        proc.getStateInformation (blob);
        auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
        if (xml == nullptr)
            return {};
        auto tree = juce::ValueTree::fromXml (*xml);
        auto ir = tree.getChildWithName ("IrSlot");
        if (! ir.isValid())
            return {};
        mutate (ir);
        auto outXml = tree.createXml();
        if (outXml == nullptr)
            return {};
        juce::MemoryBlock patched;
        juce::AudioProcessor::copyXmlToBinary (*outXml, patched);
        return patched;
    }
};
