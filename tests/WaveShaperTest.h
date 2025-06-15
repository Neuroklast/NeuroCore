#ifndef WAVESHAPERTEST_H
#define WAVESHAPERTEST_H

#include <JuceHeader.h>
#include "../src/dsp/WaveShaper.h"
#include "../src/core/EffectParameters.h"
#include <memory>

class WaveShaperTest : public juce::UnitTest
{
public:
    WaveShaperTest() : juce::UnitTest("WaveShaperTest", "DSP") {}

    void runTest() override
    {
        beginTest("tanh(x) Shaper");

        auto eval = std::make_shared<ExpressionEvaluator>();
        expect(eval->parseFormula("tanh(x)"));

        WaveShaper shaper(eval);
        juce::dsp::ProcessSpec spec{ 1.0, 4, 1 };
        shaper.prepare(spec);

        juce::AudioBuffer<float> buffer(1, 4);
        buffer.setSample(0, 0, -1.0f);
        buffer.setSample(0, 1, -0.5f);
        buffer.setSample(0, 2,  0.5f);
        buffer.setSample(0, 3,  1.0f);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        shaper.process(ctx);

        expectWithinAbsoluteError(buffer.getSample(0,0), std::tanh(-1.0f), 1e-5f);
        expectWithinAbsoluteError(buffer.getSample(0,1), std::tanh(-0.5f), 1e-5f);
        expectWithinAbsoluteError(buffer.getSample(0,2), std::tanh( 0.5f), 1e-5f);
        expectWithinAbsoluteError(buffer.getSample(0,3), std::tanh( 1.0f), 1e-5f);
    }
};

#endif // WAVESHAPERTEST_H
