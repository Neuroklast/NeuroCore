#ifndef DSLPARSERTEST_H
#define DSLPARSERTEST_H

#include <JuceHeader.h>
#include "../src/dsl/DSLParser.h"

class DSLParserTest : public juce::UnitTest
{
public:
    DSLParserTest() : juce::UnitTest("DSLParserTest", "DSL") {}

    void runTest() override
    {
        beginTest("Leerer Input");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("", blocks, aliases, params, error));
            expect(blocks.empty());
            expect(aliases.empty());
            expect(params.empty());
            expect(error.isEmpty());
        }

        beginTest("Einzelner param Block");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("param a = gain", blocks, aliases, params, error));
            expect(error.isEmpty());
            expect(blocks.empty());
            expectEquals((int)params.size(), 1);
            expectEquals(params[0].alias, juce::String("a"));
            expectEquals(params[0].name, juce::String("gain"));
        }

        beginTest("Einzelner stage Block");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("stage1: y = x * 2", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("stage"));
            expect(blocks[0].args.count("y") > 0);
        }

        beginTest("Einzelner filter lowpass Block");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("filter1: type = lowpass; cutoff = 1000", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("filter"));
        }

        beginTest("Einzelner filter bandpass (center/width)");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("filter1: type = bandpass; center = 1000; width = 500", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("filter"));
        }

        beginTest("Einzelner comp Block");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("comp1: threshold = 0.5; ratio = 2", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("comp"));
        }

        beginTest("env Block");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("env1: type = peak", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("env"));
        }

        beginTest("osc Block");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("osc1: shape = sine; freq = 440; depth = 0.5", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("osc"));
        }

        beginTest("Mehrere Bloecke zusammen");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            juce::String script =
                "param a = gain\n"
                "stage1: y = x * a\n"
                "filter1: type = lowpass; cutoff = 1000\n"
                "comp1: threshold = 0.5; ratio = 4\n";
            expect(parser.parse(script, blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 3);
            expectEquals((int)params.size(), 1);
        }

        beginTest("Ungueltiger Block (fehlender Doppelpunkt)");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("stage1 y = x", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("Unbekannter Block-Typ");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("foo1: bar = 1", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("stage ohne Formel");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("stage1: cutoff = 1000", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("filter bandpass ohne lowcut/highcut und ohne center/width");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("filter1: type = bandpass", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("comp ohne threshold/ratio");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("comp1: threshold = 0.5", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("Doppelter Block-Name");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("stage1: y = x\nstage1: y = x * 2", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("param nach Block ist Fehler");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(! parser.parse("stage1: y = x\nparam a = gain", blocks, aliases, params, error));
            expect(error.isNotEmpty());
        }

        beginTest("Kommentar-Zeilen werden ignoriert");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            juce::String script =
                "# This is a comment\n"
                "// Also a comment\n"
                "stage1: y = x\n";
            expect(parser.parse(script, blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("stage"));
        }
    }
};

#endif // DSLPARSERTEST_H
