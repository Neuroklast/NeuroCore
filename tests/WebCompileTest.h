#pragma once

#include <JuceHeader.h>
#include "../src/bridge/CompileSession.h"
#include "../src/dsl/GraphModel.h"

/** WP3: invalid DSL must not compile audio; last valid AST stays. */
class WebCompileTest : public juce::UnitTest
{
public:
    WebCompileTest() : juce::UnitTest ("WebCompile", "Bridge") {}

    void runTest() override
    {
        beginTest ("diagnosticFromError reads line from parser text");
        {
            const auto d = bridge::diagnosticFromError ("Missing ':' on line 4");
            expectEquals (d.line, 4);
            expectEquals (d.column, 1);
            expect (d.message.contains ("Missing"));
        }

        beginTest ("seed of a valid script stores lastValidAst");
        {
            bridge::CompileSession session;
            const auto seeded = session.seed ("stage1: y = tanh(x)\n");
            expect (seeded.ok);
            expect (session.lastValidAstJson().contains ("\"version\""));
            expect (session.lastValidScript().contains ("stage1"));
        }

        beginTest ("parse failure does not call apply and keeps lastValid");
        {
            bridge::CompileSession session;
            expect (session.seed ("stage1: y = x\n").ok);
            const auto before = session.lastValidAstJson();

            int applies = 0;
            const auto out = session.compile ("stage1 y = x\n", "editor",
                [&] (const juce::String&, juce::String&)
                {
                    ++applies;
                    return true;
                });

            expect (! out.ok);
            expectEquals (applies, 0);
            expect (out.diagnostics.size() >= 1);
            expectEquals (out.diagnostics[0].line, 1);
            expectEquals (session.lastValidAstJson(), before);
            expect (out.astJson == before);
        }

        beginTest ("valid compile calls apply once and replaces lastValid");
        {
            bridge::CompileSession session;
            expect (session.seed ("stage1: y = x\n").ok);
            int applies = 0;
            juce::String seen;
            const auto out = session.compile ("filter1: type = lowpass; cutoff = 800\n", "editor",
                [&] (const juce::String& script, juce::String& error)
                {
                    ++applies;
                    seen = script;
                    error.clear();
                    return true;
                });

            expect (out.ok, out.diagnostics.empty() ? juce::String() : out.diagnostics[0].message);
            expectEquals (applies, 1);
            expect (seen.contains ("filter1"));
            expect (session.lastValidAstJson().contains ("filter1"));
            dsl::GraphDocument back;
            juce::String err;
            expect (dsl::fromJson (session.lastValidAstJson(), back, err), err);
            expectEquals ((int) back.nodes.size(), 1);
            expectEquals (back.nodes[0].name, juce::String ("filter1"));
        }

        beginTest ("apply failure keeps lastValid and reports diagnostics");
        {
            bridge::CompileSession session;
            expect (session.seed ("stage1: y = x\n").ok);
            const auto before = session.lastValidAstJson();
            const auto out = session.compile ("stage1: y = tanh(x)\n", "canvas",
                [&] (const juce::String&, juce::String& error)
                {
                    error = "Error on line 2: load failed";
                    return false;
                });

            expect (! out.ok);
            expectEquals (out.diagnostics[0].line, 2);
            expectEquals (session.lastValidAstJson(), before);
            expectEquals (out.origin, juce::String ("canvas"));
        }

        beginTest ("lint does not apply and does not replace lastValid");
        {
            bridge::CompileSession session;
            expect (session.seed ("stage1: y = x\n").ok);
            const auto before = session.lastValidAstJson();
            int applies = 0;

            const auto bad = session.lint ("stage1 y = x\n");
            expect (! bad.ok);
            expect (bad.diagnostics.size() >= 1);
            expectEquals (session.lastValidAstJson(), before);

            const auto good = session.lint ("filter1: type = lowpass; cutoff = 800\n");
            expect (good.ok);
            expectEquals (session.lastValidAstJson(), before);
            expectEquals (applies, 0);
        }
    }
};
