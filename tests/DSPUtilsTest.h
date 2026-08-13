#ifndef DSPUTILSTEST_H
#define DSPUTILSTEST_H

#include <JuceHeader.h>
#include "../src/dsp/DSPUtils.h"

class DSPUtilsTest : public juce::UnitTest
{
public:
    DSPUtilsTest() : juce::UnitTest("DSPUtilsTest", "DSP") {}

    void runTest() override
    {
        beginTest("autoGainCompensate scales samples toward dry RMS");

        juce::AudioBuffer<float> dryBuffer(2, 4);
        juce::AudioBuffer<float> mixedBuffer(2, 4);
        dryBuffer.clear();
        mixedBuffer.clear();

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4; ++i)
            {
                dryBuffer.setSample(ch, i, 1.0f);
                mixedBuffer.setSample(ch, i, 0.5f);
            }

        juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
        juce::dsp::AudioBlock<float> mixedBlock(mixedBuffer);

        juce::SmoothedValue<float> smoothed;
        smoothed.reset(44100.0, 0.0);
        smoothed.setCurrentAndTargetValue(1.0f);

        juce::dsp::Gain<float> gain;
        juce::dsp::ProcessSpec spec { 44100.0, 4, 2 };
        gain.prepare(spec);
        gain.setGainLinear(1.0f);

        DSPUtils::autoGainCompensate(dryBlock, mixedBlock, smoothed, gain);

        // Mild makeup: wet quieter → 25% toward full match
        // full = 1/0.5 = 2 → corr = 1 + 0.25*(2-1) = 1.25 → 0.5*1.25 = 0.625
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4; ++i)
                expectWithinAbsoluteError(mixedBuffer.getSample(ch, i), 0.625f, 1.0e-4f);

        beginTest("autoGainCompensate keeps wet louder (punch)");

        juce::AudioBuffer<float> dry2(1, 4), wetHot(1, 4);
        for (int i = 0; i < 4; ++i)
        {
            dry2.setSample(0, i, 0.5f);
            wetHot.setSample(0, i, 1.0f); // wet 2× louder
        }
        juce::SmoothedValue<float> smHot;
        smHot.reset(44100.0, 0.0);
        smHot.setCurrentAndTargetValue(1.0f);
        juce::dsp::AudioBlock<float> dHot(dry2), wHot(wetHot);
        DSPUtils::autoGainCompensate(dHot, wHot, smHot, gain);
        // full = 0.5 → corr = 1 + 0.08*(0.5-1) = 0.96 → almost full punch kept
        for (int i = 0; i < 4; ++i)
            expectWithinAbsoluteError(wetHot.getSample(0, i), 0.96f, 1.0e-3f);

        beginTest("autoGainCompensate never mutes (residual is sanitizer-only)");

        // Architecture: dry silent + any wet level → unity, not mute.
        // Residual floor belongs solely to OutputSanitizer.
        juce::AudioBuffer<float> drySilent(1, 8);
        juce::AudioBuffer<float> wetLoud(1, 8);
        juce::AudioBuffer<float> wetTiny(1, 8);
        drySilent.clear();
        for (int i = 0; i < 8; ++i)
        {
            wetLoud.setSample(0, i, 0.2f);
            wetTiny.setSample(0, i, 1.0e-6f);
        }

        juce::SmoothedValue<float> sm2;
        sm2.reset(44100.0, 0.0);
        sm2.setCurrentAndTargetValue(1.0f);
        juce::dsp::AudioBlock<float> dB(drySilent), wB(wetLoud);
        DSPUtils::autoGainCompensate(dB, wB, sm2, gain);
        for (int i = 0; i < 8; ++i)
            expectWithinAbsoluteError(wetLoud.getSample(0, i), 0.2f, 1.0e-4f);

        juce::SmoothedValue<float> sm3;
        sm3.reset(44100.0, 0.0);
        sm3.setCurrentAndTargetValue(1.0f);
        juce::dsp::AudioBlock<float> d0(drySilent), wT(wetTiny);
        DSPUtils::autoGainCompensate(d0, wT, sm3, gain);
        for (int i = 0; i < 8; ++i)
            expectWithinAbsoluteError(wetTiny.getSample(0, i), 1.0e-6f, 1.0e-9f);
    }
};

#endif // DSPUTILSTEST_H
