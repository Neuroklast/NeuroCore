#ifndef DSLPARSERTEST_H
#define DSLPARSERTEST_H

#include <JuceHeader.h>
#include "../src/dsl/DSLParser.h"
#include "../src/dsl/NoteValues.h"
#include "../src/core/Config.h"

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

        beginTest("param note range 1/1 to 1/16 snaps through in-between steps");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse ("param a = Time [1/1, 1/16]", blocks, aliases, params, error), error);
            expectEquals ((int) params.size(), 1);
            expect (params[0].isNote);
            expect (params[0].noteLabels.size() >= 5);
            expectEquals (params[0].noteLabels.front(), juce::String ("1/1"));
            expectEquals (params[0].noteLabels.back(), juce::String ("1/16"));
            bool has8 = false, has4 = false, hasDot = false, hasTrip = false;
            for (const auto& l : params[0].noteLabels)
            {
                has8   = has8   || l == "1/8";
                has4   = has4   || l == "1/4";
                hasDot = hasDot || l == "1/8.";
                hasTrip = hasTrip || l == "1/6";
            }
            expect (has8 && has4 && hasDot && hasTrip);

            const auto grid = dsl::NoteValues::makeGrid (1.f, 0.0625f);
            expectEquals (grid.labelFromNorm (0.f), juce::String ("1/1"));
            expectEquals (grid.labelFromNorm (1.f), juce::String ("1/16"));
            const float qMs = grid.msFromNorm (0.f, 120.0);
            // 1/1 at 120 bpm = 4 beats = 2000 ms
            expectWithinAbsoluteError (qMs, 2000.f, 0.5f);
            float whole = 0.f;
            expect (dsl::NoteValues::parseToken ("1/8.", whole));
            expectWithinAbsoluteError (whole, 0.1875f, 1.0e-4f);
            expect (dsl::NoteValues::parseToken ("1/8t", whole));
            expectWithinAbsoluteError (whole, 1.f / 12.f, 1.0e-4f);
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

        beginTest("widen and stereo alias parse");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("widen1: width = 0.7; delay = 14; bass = 140", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("widen"));
            blocks.clear();
            expect(parser.parse("stereo1: width = 0.5", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals(blocks[0].type, juce::String("widen"));
        }

        beginTest("ott block parses");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect(parser.parse("ott1: depth = 0.5; time = 0.3", blocks, aliases, params, error));
            expect(error.isEmpty());
            expectEquals((int)blocks.size(), 1);
            expectEquals(blocks[0].type, juce::String("ott"));
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

        beginTest("inline hash comments are stripped");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse (
                "stage1: y = tanh(x)  # tube-ish\n"
                "filter1: type = lowpass; cutoff = 1000  // cab\n",
                blocks, aliases, params, error), error);
            expectEquals ((int) blocks.size(), 2);
            expectEquals (blocks[0].args["y"], juce::String ("tanh(x)"));
        }

        beginTest("formatBlockSummary for stage and filter");
        {
            dsl::BlockDesc stage;
            stage.type = "stage";
            stage.name = "stage1";
            stage.args["y"] = "tanh(x * a)";
            expectEquals(dsl::formatBlockSummary(stage), juce::String("y = tanh(x * a)"));

            dsl::BlockDesc filter;
            filter.type = "filter";
            filter.name = "filter1";
            filter.args["type"] = "lowpass";
            filter.args["cutoff"] = "1000";
            expect(dsl::formatBlockSummary(filter).contains("lowpass"));
            expect(dsl::formatBlockSummary(filter).contains("cutoff=1000"));
        }

        beginTest("top-level stage belongs to main");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse ("stage1: y = x * 2", blocks, aliases, params, error));
            expect (error.isEmpty());
            expectEquals ((int) blocks.size(), 1);
            expectEquals (blocks[0].busName, juce::String ("main"));
        }

        beginTest("bus section assigns following stage");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            const juce::String script =
                "stage1: y = x\n"
                "bus dirt:\n"
                "stage2: y = tube(x, a)\n";
            expect (parser.parse (script, blocks, aliases, params, error), error);
            expect (error.isEmpty());
            expectEquals ((int) blocks.size(), 3);
            expectEquals (blocks[0].type, juce::String ("stage"));
            expectEquals (blocks[0].busName, juce::String ("main"));
            expectEquals (blocks[1].type, juce::String ("bus"));
            expectEquals (blocks[1].name, juce::String ("dirt"));
            expectEquals (blocks[2].type, juce::String ("stage"));
            expectEquals (blocks[2].busName, juce::String ("dirt"));
        }

        beginTest("send and out parse args");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            const juce::String script =
                "bus dirt:\n"
                "send: in = 1; main = 0.35\n"
                "stage1: y = x\n"
                "out: main = 1; dirt = c\n";
            expect (parser.parse (script, blocks, aliases, params, error), error);
            expect (error.isEmpty());
            expectEquals ((int) blocks.size(), 4);
            expectEquals (blocks[1].type, juce::String ("send"));
            expectEquals (blocks[1].busName, juce::String ("dirt"));
            expectEquals (blocks[1].args["in"], juce::String ("1"));
            expectEquals (blocks[1].args["main"], juce::String ("0.35"));
            expectEquals (blocks[3].type, juce::String ("out"));
            expectEquals (blocks[3].args["main"], juce::String ("1"));
            expectEquals (blocks[3].args["dirt"], juce::String ("c"));
        }

        beginTest("send at top level is an error");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (! parser.parse ("send: in = 1\nstage1: y = x", blocks, aliases, params, error));
            expect (error.containsIgnoreCase ("send"));
        }

        beginTest("reserved bus names rejected");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (! parser.parse ("bus main:\nstage1: y = x", blocks, aliases, params, error));
            expect (error.isNotEmpty());
            expect (! parser.parse ("bus in:\nstage1: y = x", blocks, aliases, params, error));
            expect (error.isNotEmpty());
        }

        beginTest("thirteenth named bus rejected");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            juce::String script;
            for (int i = 1; i <= 13; ++i)
                script << "bus a" << juce::String (i) << ":\nstage" << juce::String (i) << ": y = x\n";
            expect (! parser.parse (script, blocks, aliases, params, error));
            expect (error.containsIgnoreCase ("bus"));
        }

        beginTest("octaver and vocoder blocks parse");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse (
                "octaver1: sub = 0.6; up = 0.2; mix = 0.7; tone = 400\n"
                "vocoder1: bands = 8; mix = 0.8; q = 2; formant = 1; dry = 0.2\n",
                blocks, aliases, params, error), error);
            expectEquals ((int) blocks.size(), 2);
            expectEquals (blocks[0].type, juce::String ("octaver"));
            expectEquals (blocks[1].type, juce::String ("vocoder"));
            expectEquals (blocks[1].args.at ("bands"), juce::String ("8"));
        }

        beginTest("split expander ignores Split: inside comments");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse (
                "# d Split: 90 to 220, default 150\n"
                "param d = Split [90, 220]\n"
                "xover1: f1 = d; f2 = 2500\n"
                "stage1: y = x\n",
                blocks, aliases, params, error), error);
            expect (error.isEmpty(), error);
            expect (blocks.size() >= 2);
            expectEquals (blocks[0].type, juce::String ("xover"));
        }

        beginTest("split midside expands to encode / channel / decode");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse (
                "split1: type = midside {\n"
                "  mid { stage1: y = x * b }\n"
                "  side { stage2: y = x * a }\n"
                "}\n",
                blocks, aliases, params, error), error);
            expect (blocks.size() >= 4);
            expectEquals (blocks[0].type, juce::String ("ms"));
            expectEquals (blocks[0].args.at ("mode"), juce::String ("encode"));
            expectEquals (blocks[1].args.at ("channel"), juce::String ("mid"));
            expectEquals (blocks[2].args.at ("channel"), juce::String ("side"));
            expectEquals (blocks.back().args.at ("mode"), juce::String ("decode"));
        }

        beginTest("split leftright stamps left and right");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (parser.parse (
                "split1: type = leftright {\n"
                "  left { stage1: y = tube(x, a) }\n"
                "  right { stage2: y = tube(x, b) }\n"
                "}\n",
                blocks, aliases, params, error), error);
            expectEquals ((int) blocks.size(), 2);
            expectEquals (blocks[0].args.at ("channel"), juce::String ("left"));
            expectEquals (blocks[1].args.at ("channel"), juce::String ("right"));
        }

        beginTest("block after out is an error");
        {
            dsl::DSLParser parser;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String error;
            expect (! parser.parse ("stage1: y = x\nout: main = 1\nstage2: y = x", blocks, aliases, params, error));
            expect (error.containsIgnoreCase ("out"));
        }
    }
};

#endif // DSLPARSERTEST_H
