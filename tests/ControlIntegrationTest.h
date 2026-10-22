#pragma once
#include <JuceHeader.h>
#include "../src/dsl/SignalChain.h"
#include "../src/bridge/GraphOps.h"
#include "../src/bridge/AstJson.h"

class ControlIntegrationTest : public juce::UnitTest
{
public:
    ControlIntegrationTest() : juce::UnitTest ("ControlIntegration", "DSP") {}
    void runTest() override
    {
        beginTest ("audio Join accepts inA and inB through the real graph transaction");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("stage1: y = x\nbus dirt:\n stage2: y = x\njoin1: mix = 0.5\n", doc, error), error);
            expect (bridge::applyGraphOp (doc, bridge::GraphOp::connect ("stage1", "out", "join1", "inA"), error), error);
            expect (bridge::applyGraphOp (doc, bridge::GraphOp::connect ("stage2", "out", "join1", "inB"), error), error);
            dsl::GraphDocument restored;
            expect (dsl::fromJson (dsl::toJson (doc), restored, error), error);
            dsl::SignalChain chain;
            expect (chain.loadScript (dsl::emit (restored), error), error);
        }
        beginTest ("control A/B rejects audio while accepting real modulation");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("random1: min = 0.5; max = 0.5\nfilter1: cutoff = 800\nmodmix1: levela = 1\n", doc, error), error);
            expect (! bridge::applyGraphOp (doc, bridge::GraphOp::connect ("filter1", "out", "modmix1", "inA"), error));
            expect (error.containsIgnoreCase ("mod source"), error);
            expect (bridge::applyGraphOp (doc, bridge::GraphOp::connect ("random1", "out", "modmix1", "inA"), error), error);
        }
        beginTest ("depth and polarity are preserved through every control-lane consumer");
        for (const auto& type : { juce::String ("modmath"), juce::String ("modmix"), juce::String ("samplehold") })
        {
            const auto node = type + "1";
            const auto args = type == "samplehold" ? "; rate = 1000; slew = 0; quantize = 0"
                                                   : type == "modmath" ? "; mode = add; amount = 1" : "; levela = 1; levelb = 0";
            expectWithinAbsoluteError (render (
                "random1: rate = 1000; min = 0.5; max = 0.5; depth = 0.5; polarity = invert\n"
                + node + ": ina = random1; mix = 1" + args + "\nstage1: y = x * " + node + "\n"),
                -0.125f, 1.0e-4f, type);
        }
        beginTest ("each stage applies its own depth/polarity exactly once");
        expectWithinAbsoluteError (render (
            "random1: rate = 1000; min = 0.5; max = 0.5; depth = 0.5; polarity = invert\n"
            "modmath1: ina = random1; mode = add; depth = 0.4; polarity = invert\n"
            "modmix1: ina = modmath1; levela = 1; levelb = 0\n"
            "stage1: y = x * modmix1\n"), 0.05f, 1.0e-4f);
    }
private:
    float render (const juce::String& script)
    {
        dsl::SignalChain chain;
        juce::String error;
        expect (chain.loadScript (script, error), error);
        chain.prepare ({48000.0, 256, 1});
        juce::AudioBuffer<float> buffer (1,256);
        for (int block=0; block<8; ++block)
        {
            juce::FloatVectorOperations::fill (buffer.getWritePointer (0), 0.5f, 256);
            chain.processBlock (buffer);
        }
        return buffer.getSample (0,255);
    }
};
