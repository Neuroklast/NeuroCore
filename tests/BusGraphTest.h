#ifndef BUSGRAPHTEST_H
#define BUSGRAPHTEST_H

#include <JuceHeader.h>
#include "../src/dsl/BusGraph.h"
#include "../src/dsl/DSLParser.h"

class BusGraphTest : public juce::UnitTest
{
public:
    BusGraphTest() : juce::UnitTest ("BusGraph", "DSL") {}

    void runTest() override
    {
        beginTest ("serial script is a single main bus");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse ("stage1: y = x\nfilter1: type = lowpass; cutoff = 1000",
                                  blocks, aliases, params, error));
            dsl::BusGraph g;
            expect (dsl::buildBusGraph (blocks, g, error), error);
            expectEquals ((int) g.buses.size(), 1);
            expectEquals (g.buses[0].name, juce::String ("main"));
            expectEquals ((int) g.buses[0].blockIndices.size(), 2);
            expect (! g.hasExplicitOut());
        }

        beginTest ("named bus and send from in");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            const juce::String script =
                "stage1: y = x\n"
                "bus dirt:\n"
                "send: in = 1\n"
                "stage2: y = x * 0.5\n"
                "out: main = 1; dirt = 1\n";
            expect (parser.parse (script, blocks, aliases, params, error), error);
            dsl::BusGraph g;
            expect (dsl::buildBusGraph (blocks, g, error), error);
            expectEquals ((int) g.buses.size(), 2);
            expectEquals (g.buses[1].name, juce::String ("dirt"));
            expectEquals ((int) g.buses[1].sends.size(), 1);
            expectEquals (g.buses[1].sends[0].sourceIndex, dsl::kReservedBusIn);
            expect (g.hasExplicitOut());
            expectEquals ((int) g.outTaps.size(), 2);
        }

        beginTest ("forward send is rejected");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            const juce::String script =
                "bus early:\n"
                "send: late = 1\n"
                "stage1: y = x\n"
                "bus late:\n"
                "send: in = 1\n"
                "stage2: y = x\n";
            expect (parser.parse (script, blocks, aliases, params, error), error);
            dsl::BusGraph g;
            expect (! dsl::buildBusGraph (blocks, g, error));
            expect (error.containsIgnoreCase ("forward") || error.containsIgnoreCase ("send"));
        }

        beginTest ("unknown send source is rejected");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse ("bus dirt:\nsend: nope = 1\nstage1: y = x\n",
                                  blocks, aliases, params, error));
            dsl::BusGraph g;
            expect (! dsl::buildBusGraph (blocks, g, error));
            expect (error.containsIgnoreCase ("unknown"));
        }

        beginTest ("mixdown 0.5 + 0.5");
        {
            juce::AudioBuffer<float> a (1, 2), b (1, 2), dest (1, 2);
            a.setSample (0, 0, 1.0f); a.setSample (0, 1, 0.0f);
            b.setSample (0, 0, 1.0f); b.setSample (0, 1, 2.0f);
            dest.clear();
            std::vector<juce::AudioBuffer<float>*> srcs { &a, &b };
            std::vector<float> gains { 0.5f, 0.5f };
            dsl::mixdown (dest, srcs, gains);
            expectWithinAbsoluteError (dest.getSample (0, 0), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (dest.getSample (0, 1), 1.0f, 1.0e-6f);
        }
    }
};

#endif
