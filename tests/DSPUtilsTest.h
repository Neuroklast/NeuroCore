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
        beginTest("autoGainCompensate scales samples to dry RMS");

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

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4; ++i)
                expectWithinAbsoluteError(mixedBuffer.getSample(ch, i), 1.0f, 1.0e-5f);
    }
};

#endif // DSPUTILSTEST_H
