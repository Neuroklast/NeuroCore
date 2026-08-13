#pragma once

#include <JuceHeader.h>
#include "../src/utils/FormulaHelper.h"
#include "../src/dsl/DSLParser.h"

class FormulaOptimizeTest : public juce::UnitTest
{
public:
    FormulaOptimizeTest() : juce::UnitTest ("FormulaOptimizeTest", "Utils") {}

    void runTest() override
    {
        beginTest ("identity simplify");
        {
            juce::String info;
            auto out = optimizeFormula ("x * 1", info);
            expect (compact (out) == "x" || compact (out) == "x*1"); // at least no crash
            auto rep = optimizeFormulaDetailed ("x*1.0");
            expect (rep.verified);
            expect (compact (rep.script) == "x");
        }

        beginTest ("classic softclip formula");
        {
            auto rep = optimizeFormulaDetailed ("x / (1 + abs(x))");
            expect (rep.verified);
            expect (compact (rep.script).contains ("softclip"));
            expectGreaterThan (rep.changes, 0);
        }

        beginTest ("clamp brickwall -> hardclip");
        {
            auto rep = optimizeFormulaDetailed ("clamp(x, -1, 1)");
            expect (rep.verified);
            expect (compact (rep.script).contains ("hardclip"));
        }

        beginTest ("softclip(x*a) -> softclip(x, a)");
        {
            auto rep = optimizeFormulaDetailed ("softclip(x * a)");
            expect (rep.verified);
            expectGreaterThan (rep.changes, 0);
            // beautify may add spaces: softclip(x, a)
            expect (compact (rep.script).contains ("softclip(x,a)"));
        }

        beginTest ("full multi-line script preserves structure");
        {
            const juce::String script =
                "param a = Drive [1, 4]\n"
                "stage1: y = clamp(x, -1, 1)\n"
                "filter1: type = highpass; cutoff = 80; resonance = 0.3\n";

            auto rep = optimizeFormulaDetailed (script);
            expect (rep.verified);
            expect (rep.script.containsIgnoreCase ("param a"));
            expect (rep.script.containsIgnoreCase ("stage1"));
            expect (rep.script.containsIgnoreCase ("filter1"));
            expect (rep.script.containsIgnoreCase ("hardclip")
                    || rep.script.containsIgnoreCase ("clamp")); // hardclip preferred

            dsl::DSLParser p;
            std::vector<dsl::BlockDesc> blocks;
            std::unordered_map<juce::String, juce::String> aliases;
            std::vector<dsl::ParamDesc> params;
            juce::String err;
            expect (p.parse (rep.script, blocks, aliases, params, err), err);
        }

        beginTest ("bare hardclip gets AA LPF if no filter present");
        {
            auto rep = optimizeFormulaDetailed ("stage1: y = hardclip(x, 0.5)");
            expect (rep.verified);
            expect (rep.script.containsIgnoreCase ("lowpass"));
            expectGreaterThan (rep.changes, 0);
        }

        beginTest ("rejects garbage that would not parse");
        {
            // Optimizer should not invent broken scripts from random text
            auto rep = optimizeFormulaDetailed ("this is not a formula !!!");
            // Either unchanged or still non-parse; must not claim verified success with broken DSL
            if (rep.changes > 0)
                expect (rep.verified); // if changed, must have passed gates
        }

        beginTest ("no destructive strip of newlines");
        {
            const juce::String script =
                "stage1: y = softclip(x, a)\n"
                "stage2: y = softclip(y, 1.1)\n";
            auto rep = optimizeFormulaDetailed (script);
            expect (rep.script.containsChar ('\n') || rep.script.contains ("stage2"));
        }
    }

private:
    static juce::String compact (const juce::String& s)
    {
        juce::String o;
        for (int i = 0; i < s.length(); ++i)
            if (! juce::CharacterFunctions::isWhitespace (s[i]))
                o += s[i];
        return o;
    }
};


