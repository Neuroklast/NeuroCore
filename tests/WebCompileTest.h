#pragma once

#include <JuceHeader.h>
#include "../src/bridge/CompileSession.h"
#include "../src/bridge/AstJson.h"
#include "../src/dsl/GraphModel.h"

/** WP3: invalid DSL must not compile audio; last valid AST stays. */
class WebCompileTest : public juce::UnitTest
{
public:
    WebCompileTest() : juce::UnitTest ("WebCompile", "Bridge") {}

    void runTest() override
    {
        beginTest ("multiline expressions compile and preserve source text");
        {
            bridge::CompileSession session;
            const juce::String script = "stage1: y = tanh(\n x * 2 # drive\n)\nout: main = 1\n";
            const auto result = session.seed (script);
            expect (result.ok, "Multiline stage expression rejected");
        }

        beginTest ("graph edits preserve untouched multiline source and comments");
        {
            const juce::String source = "stage1: y = tanh(\n x * 2 # drive\n)\n# Output trim\nout: main = 1\n";
            dsl::GraphDocument doc;
            juce::String error;
            expect(dsl::parse(source, doc, error), error);
            if (doc.nodes.size() == 2) {
                doc.nodes.back().args["gain"] = "-3";
                const auto emitted = dsl::emit(doc);
                expect(emitted.contains("tanh(\n x * 2 # drive\n)"));
                expect(emitted.contains("# Output trim"));
            }
        }

        beginTest ("factory graph emission retains DSP semantics");
        {
            const auto file = juce::File(NEUROKORE_RESOURCES_DIR).getChildFile("factory_presets.json");
            const auto catalog = juce::JSON::parse(file);
            int checked = 0, broken = 0;
            juce::String first;
            if (auto* rows = catalog.getArray()) for (const auto& row : *rows) {
                dsl::GraphDocument original, emitted;
                juce::String error;
                const auto script = row.getProperty("script", "").toString();
                ++checked;
                if (!dsl::parse(script, original, error) || !dsl::parse(dsl::emit(original), emitted, error)
                    || !dsl::semanticallyEqual(original, emitted)) {
                    ++broken;
                    if (first.isEmpty()) first = row.getProperty("name", "").toString() + ": " + error;
                }
            }
            expect(checked > 300);
            expectEquals(broken, 0, first);
        }

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
