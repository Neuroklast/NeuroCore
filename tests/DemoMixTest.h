#pragma once

#include <JuceHeader.h>
#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"

class DemoMixTest : public juce::UnitTest
{
public:
    DemoMixTest() : juce::UnitTest ("DemoMix", "Licensing") {}

    void runTest() override
    {
        beginTest ("demo lock forces delayed dry while the mix parameter stays wet");
        {
            NeuroKoreAudioProcessor proc;
            proc.setLiveMode (false);
            proc.setPlayConfigDetails (2, 2, 48000.0, 128);
            auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                proc.apvts.getParameter (EffectParameters::oversampling));
            expect (choice != nullptr);
            choice->setValueNotifyingHost (choice->convertTo0to1 (2.f));
            juce::String err;
            expect (proc.setFormula ("stage1: y = 0", err), err);
            proc.prepareToPlay (48000.0, 128);
            if (auto* mix = proc.apvts.getParameter (EffectParameters::dryWet))
            {
                mix->setValueNotifyingHost (1.f);
                expect (mix->getValue() > 0.9f);
            }
#if defined (NEUROKORE_SKIP_LICENSE_ENFORCEMENT)
            proc.setDemoMixLockedForTest (true);
#endif
            expect (proc.isDemoMixLocked());

            const int lat = proc.getLatencySamples();
            juce::AudioBuffer<float> buf (2, 128);
            juce::MidiBuffer midi;
            for (int b = 0; b < 48; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
            }
            buf.clear();
            buf.setSample (0, 0, 1.f);
            buf.setSample (1, 0, 1.f);
            proc.processBlock (buf, midi);
            int peakAt = -1;
            float peak = 0.f;
            int sample = 0;
            for (int b = 0; b < 16; ++b)
            {
                if (b > 0)
                {
                    buf.clear();
                    proc.processBlock (buf, midi);
                }
                for (int i = 0; i < 128; ++i, ++sample)
                {
                    const float a = std::abs (buf.getSample (0, i));
                    if (a > peak)
                    {
                        peak = a;
                        peakAt = sample;
                    }
                }
            }
            expect (peak > 0.25f, "locked demo must emit delayed dry, peak=" + juce::String (peak, 3));
            if (lat > 0)
                expect (std::abs (peakAt - lat) <= 2,
                        "demo-lock peakAt=" + juce::String (peakAt) + " lat=" + juce::String (lat));
            if (auto* mix = proc.apvts.getParameter (EffectParameters::dryWet))
                expect (mix->getValue() > 0.9f, "host mix parameter must stay wet");
            proc.releaseResources();
        }
    }
};
