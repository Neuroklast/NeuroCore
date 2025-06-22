#ifndef SIGNALCHAINTEST_H
#define SIGNALCHAINTEST_H

#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"

class SignalChainTest : public juce::UnitTest
{
public:
    SignalChainTest() : juce::UnitTest("SignalChainTest", "DSL") {}

    void runTest() override
    {
        beginTest("Parse and process simple chain");
        dsl::SignalChain chain;
        juce::String err;
        juce::String script = "stage1: y = x * a\n";
        expect(chain.loadScript(script, err));
        expect(err.isEmpty());
        juce::dsp::ProcessSpec spec{44100.0, 4, 1};
        chain.prepare(spec);
        juce::AudioBuffer<float> buffer(1,4);
        buffer.clear();
        buffer.setSample(0,0,1.0f);
        std::array<float,4> params{2.f,0.f,0.f,0.f};
        chain.processBlock(buffer, params);
        expectWithinAbsoluteError(buffer.getSample(0,0), 2.0f, 1e-5f);

        beginTest("Filter block with high cutoff");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nfilter1: type = lowpass; cutoff = 20000";
            expect(c.loadScript(sc, e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1,2); buf.clear(); buf.setSample(0,0,1.0f);
            c.processBlock(buf, {0,0,0,0});
            expectGreaterOrEqual(buf.getSample(0,0), 0.5f);
        }

        beginTest("Compressor reduces level");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\ncomp1: threshold = 0.5; ratio = 2";
            expect(c.loadScript(sc, e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1,1); buf.setSample(0,0,1.0f);
            c.processBlock(buf, {0,0,0,0});
            expectLessThan(buf.getSample(0,0), 1.0f);
        }

        beginTest("Envelope follower pass-through");
        {
            dsl::SignalChain c;
            juce::String e;
            juce::String sc = "stage1: y = x\nenv1: type = peak";
            expect(c.loadScript(sc, e));
            c.prepare(spec);
            juce::AudioBuffer<float> buf(1,1); buf.setSample(0,0,0.5f);
            c.processBlock(buf, {0,0,0,0});
            expectWithinAbsoluteError(buf.getSample(0,0), 0.5f, 1e-5f);
        }
    }
};

#endif // SIGNALCHAINTEST_H
