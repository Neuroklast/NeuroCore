#pragma once

#include <JuceHeader.h>
#include "../src/core/PluginProcessor.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "TestHelpers.h"
#include <cmath>

#ifndef NEUROKORE_RESOURCES_DIR
#define NEUROKORE_RESOURCES_DIR "resources"
#endif

class FactoryLoudnessTest : public juce::UnitTest
{
public:
    FactoryLoudnessTest() : juce::UnitTest ("FactoryLoudnessTest", "Presets") {}

    void runTest() override
    {
        beginTest ("every factory insert meets the loudness floor");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            expect (lib.getEntries().size() > 100);

            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 512);
            proc.setNonRealtime (true); // This is an offline measurement, not a callback-budget test.
            proc.prepareToPlay (48000.0, 512);

            int insertN = 0, sendN = 0, quietN = 0;
            juce::String quietList;
            for (int i = 0; i < (int) lib.getEntries().size(); ++i)
            {
                const auto& e = lib.getEntries()[(size_t) i];
                juce::String err;
                if (! lib.applyPreset (proc, i, err))
                {
                    expect (false, "apply failed: " + e.name + " " + err);
                    continue;
                }

                float peak = 0.f;
                double acc = 0.0;
                int n = 0;
                juce::AudioBuffer<float> buf (2, 512);
                juce::MidiBuffer midi;
                uint32_t rng = 0xA341316Cu ^ (uint32_t) i;
                for (int b = 0; b < 56; ++b)
                {
                    for (int s = 0; s < 512; ++s)
                    {
                        rng = rng * 1664525u + 1013904223u;
                        const float pink = ((float) (rng >> 16) / 32768.f - 1.f) * 0.18f;
                        const float t = (float) (b * 512 + s) / 48000.f;
                        const float sine = 0.28f * std::sin (2.f * juce::MathConstants<float>::pi * 440.f * t);
                        const float x = sine + pink;
                        buf.setSample (0, s, x);
                        buf.setSample (1, s, x * 0.92f);
                    }
                    proc.processBlock (buf, midi);
                    if (b < 32)
                        continue;
                    for (int s = 0; s < 512; ++s)
                    {
                        const float v = 0.5f * (buf.getSample (0, s) + buf.getSample (1, s));
                        peak = juce::jmax (peak, std::abs (v));
                        acc += (double) v * (double) v;
                        ++n;
                    }
                }
                const float rms = n > 0 ? (float) std::sqrt (acc / (double) n) : 0.f;
                const bool send = isSendLike (e.category, e.name);
                logMessage (e.name + "  [" + e.category + (send ? "/send" : "/ins") + "]  peak="
                            + juce::String (peak, 3) + "  rms=" + juce::String (rms, 4));
                if (send)
                {
                    ++sendN;
                    continue;
                }
                ++insertN;
                // Gate- and spectrum-dependent effects need a meaningful but
                // lower floor than broadband unity-gain inserts. They still
                // have to produce finite, measurable output for this probe.
                const bool midiGated = e.requiredInputs.contains ("midi", true)
                    || e.script.containsIgnoreCase ("source = midi")
                    || e.script.containsIgnoreCase ("gate = midi");
                const bool rhythmic = e.script.containsIgnoreCase ("stepseq1:");
                const bool narrowBand = e.script.containsIgnoreCase ("type = bandpass");
                const bool loFi = e.category.equalsIgnoreCase ("Lo-Fi");
                const bool filter = e.category.equalsIgnoreCase ("Filter");
                const float peakFloor = midiGated ? 0.02f : rhythmic ? 0.07f
                                      : narrowBand ? 0.09f : loFi ? 0.11f
                                      : filter ? 0.08f : 0.12f;
                // MIDI-gated inserts often leak only a dry floor (e.g. 8 %)
                // when the probe has no notes. 0.006 rms is still measurable.
                const float rmsFloor = midiGated ? 0.006f : rhythmic ? 0.025f
                                     : narrowBand ? 0.030f : loFi ? 0.035f
                                     : filter ? 0.025f : 0.038f;
                if (peak < peakFloor || rms < rmsFloor)
                {
                    ++quietN;
                    quietList += e.name + " p=" + juce::String (peak, 3)
                               + " r=" + juce::String (rms, 4) + "; ";
                }
            }
            expect (insertN > 40, "should classify many inserts");
            expectEquals (quietN, 0, "quiet inserts: " + quietList);
            juce::ignoreUnused (sendN);
        }
    }

private:
    static bool isSendLike (const juce::String& category, const juce::String& name)
    {
        if (category.equalsIgnoreCase ("Delay") || category.equalsIgnoreCase ("Reverb"))
            return true;
        return name.containsIgnoreCase ("Send")
            || name.equalsIgnoreCase ("Side Delay")
            || name.equalsIgnoreCase ("Side Hall")
            || name.equalsIgnoreCase ("Width Delay")
            || name.equalsIgnoreCase ("Slap Double");
    }
};
