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

        shaper.processBlock(buffer);

        expectWithinAbsoluteError(buffer.getSample(0,0), std::tanh(-1.0f), 1e-5f);
        expectWithinAbsoluteError(buffer.getSample(0,1), std::tanh(-0.5f), 1e-5f);
        expectWithinAbsoluteError(buffer.getSample(0,2), std::tanh( 0.5f), 1e-5f);
        expectWithinAbsoluteError(buffer.getSample(0,3), std::tanh( 1.0f), 1e-5f);

        beginTest("Crossfade Start");

        auto evalOld = std::make_shared<ExpressionEvaluator>();
        auto evalNew = std::make_shared<ExpressionEvaluator>();
        expect(evalOld->parseFormula("x"));
        expect(evalNew->parseFormula("2*x"));

        WaveShaper fadeShaper(evalOld);
        juce::dsp::ProcessSpec crossSpec{ 44100.0, 1, 1 };
        fadeShaper.prepare(crossSpec);

        juce::AudioBuffer<float> fadeBuffer(1, 1);

        fadeBuffer.setSample(0, 0, 1.0f);
        fadeShaper.startFunctionCrossfade(evalNew);
        fadeShaper.processBlock(fadeBuffer);
        expectWithinAbsoluteError(fadeBuffer.getSample(0,0), 1.0f, 1e-5f);

        const int steps = static_cast<int>(Config::kCrossfadeTime * crossSpec.sampleRate) + 1;
        for (int i = 0; i < steps; ++i)
        {
            fadeBuffer.setSample(0, 0, 1.0f);
            fadeShaper.processBlock(fadeBuffer);
        }

        expectWithinAbsoluteError(fadeBuffer.getSample(0,0), 2.0f, 1e-3f);
    }
};

#endif // WAVESHAPERTEST_H
