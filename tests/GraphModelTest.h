#ifndef GRAPHMODELTEST_H
#define GRAPHMODELTEST_H

#include <JuceHeader.h>
#include <cmath>
#include "../src/dsl/GraphModel.h"
#include "../src/utils/FactoryPresetLibrary.h"

#ifndef NEUROKORE_RESOURCES_DIR
#define NEUROKORE_RESOURCES_DIR "resources"
#endif

class GraphModelTest : public juce::UnitTest
{
public:
    GraphModelTest() : juce::UnitTest ("GraphModel", "DSL") {}

    void runTest() override
    {
        beginTest ("simple chain parse, emit, parse, semanticallyEqual");
        {
            const juce::String script =
                "param a = Drive [0.0, 2.0]\n"
                "filter1: type = lowpass; cutoff = 800; resonance = 0.7  # tone\n"
                "stage1: y = tanh(x * a)\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expectEquals ((int) doc.params.size(), 1);
            expectEquals (doc.params[0].alias, juce::String ("a"));
            expectEquals (doc.params[0].name, juce::String ("Drive"));
            expectEquals ((int) doc.nodes.size(), 2);
            expectEquals (doc.nodes[0].type, juce::String ("filter"));
            expectEquals (doc.nodes[0].name, juce::String ("filter1"));
            expectEquals (doc.nodes[0].busName, juce::String ("main"));
            expectEquals (doc.nodes[0].trailingComment, juce::String ("tone"));
            expectEquals (doc.nodes[1].type, juce::String ("stage"));
            expect (doc.nodes[1].args.count ("y") == 1);
            expectEquals (doc.nodes[1].args.at ("y"), juce::String ("tanh(x * a)"));

            const juce::String emitted = dsl::emit (doc);
            expect (emitted.contains ("param a = Drive"));
            expect (emitted.contains ("filter1:"));
            expect (emitted.contains ("stage1:"));
            expect (emitted.contains ("y = tanh(x * a)"));

            dsl::GraphDocument again;
            expect (dsl::parse (emitted, again, error), error);
            expect (dsl::semanticallyEqual (doc, again));
        }

        beginTest ("bus script: bus dirt send out");
        {
            const juce::String script =
                "stage1: y = softclip(x, 1.2)\n"
                "bus dirt:\n"
                "  send: in = 1\n"
                "  stage2: y = tube(x, a)\n"
                "out: main = 1; dirt = c\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expectEquals ((int) doc.nodes.size(), 5);
            expectEquals (doc.nodes[0].type, juce::String ("stage"));
            expectEquals (doc.nodes[0].busName, juce::String ("main"));
            expectEquals (doc.nodes[1].type, juce::String ("bus"));
            expectEquals (doc.nodes[1].name, juce::String ("dirt"));
            expect (doc.nodes[1].busName.isEmpty());
            expectEquals (doc.nodes[2].type, juce::String ("send"));
            expectEquals (doc.nodes[2].name, juce::String ("send"));
            expectEquals (doc.nodes[2].busName, juce::String ("dirt"));
            expectEquals (doc.nodes[2].args.at ("in"), juce::String ("1"));
            expectEquals (doc.nodes[3].type, juce::String ("stage"));
            expectEquals (doc.nodes[3].busName, juce::String ("dirt"));
            expectEquals (doc.nodes[4].type, juce::String ("out"));
            expectEquals (doc.nodes[4].name, juce::String ("out"));

            const juce::String emitted = dsl::emit (doc);
            expect (emitted.contains ("bus dirt:"));
            expect (emitted.contains ("send:"));
            expect (emitted.contains ("out:"));
            expect (emitted.contains ("in = 1"));
            expect (emitted.contains ("main = 1"));
            expect (emitted.contains ("dirt = c"));

            dsl::GraphDocument again;
            expect (dsl::parse (emitted, again, error), error);
            expect (dsl::semanticallyEqual (doc, again));
        }

        beginTest ("moveNode reorders two filters");
        {
            const juce::String script =
                "filter1: type = lowpass; cutoff = 800\n"
                "filter2: type = highpass; cutoff = 120\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expectEquals ((int) doc.nodes.size(), 2);
            expectEquals (doc.nodes[0].name, juce::String ("filter1"));
            expectEquals (doc.nodes[1].name, juce::String ("filter2"));

            dsl::moveNode (doc, 0, 1);
            expectEquals (doc.nodes[0].name, juce::String ("filter2"));
            expectEquals (doc.nodes[1].name, juce::String ("filter1"));

            const juce::String emitted = dsl::emit (doc);
            const int i2 = emitted.indexOf ("filter2:");
            const int i1 = emitted.indexOf ("filter1:");
            expect (i2 >= 0 && i1 >= 0);
            expect (i2 < i1, "emit must contain swapped filter order");
        }

        beginTest ("setNodeArg binds a knob letter and editableArgKeys lists stage y");
        {
            const juce::String script = "filter1: type = lowpass; cutoff = 800\n";
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expect (dsl::setNodeArg (doc, 0, "cutoff", "a"));
            expectEquals (doc.nodes[0].args.at ("cutoff"), juce::String ("a"));
            const auto keys = dsl::editableArgKeys (doc.nodes[0]);
            expect (keys.contains ("cutoff"));
            expect (keys.contains ("type"));
            expect (dsl::setNodeArg (doc, 0, "cutoff", {}));
            expect (doc.nodes[0].args.count ("cutoff") == 0);

            dsl::GraphDocument stage;
            expect (dsl::parse ("stage1: y = tube(x, 1.2)\n", stage, error), error);
            const auto sk = dsl::editableArgKeys (stage.nodes[0]);
            expect (sk.contains ("y"));
            expect (dsl::setNodeArg (stage, 0, "y", "tube(x, b)"));
            expectEquals (stage.nodes[0].args.at ("y"), juce::String ("tube(x, b)"));
            const auto binds = dsl::knobBindings (stage.nodes[0]);
            expectEquals ((int) binds.size(), 1);
            expectEquals (binds[0].knobIndex, 1);
            expect (! binds[0].wholeValue);
        }

        beginTest ("@x,y layout survives emit/parse and connectAudio reorders");
        {
            const juce::String script =
                "filter1: type = lowpass; cutoff = 800  # tone @40,80\n"
                "filter2: type = highpass; cutoff = 120\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expectEquals ((int) doc.nodes.size(), 2);
            expect (std::abs (doc.nodes[0].x - 40.f) < 0.01f);
            expect (std::abs (doc.nodes[0].y - 80.f) < 0.01f);
            expectEquals (doc.nodes[0].trailingComment, juce::String ("tone"));
            expect (! dsl::hasAllPositions (doc));

            dsl::setPosition (doc, 1, 200.f, 80.f);
            const auto emitted = dsl::emit (doc);
            expect (emitted.contains ("@40"));
            expect (emitted.contains ("@200"));

            dsl::GraphDocument again;
            expect (dsl::parse (emitted, again, error), error);
            expect (dsl::semanticallyEqual (doc, again));
            expect (std::abs (again.nodes[1].x - 200.f) < 0.1f);

            expect (dsl::connectAudio (doc, 1, 0, error), error);
            expectEquals (doc.nodes[0].name, juce::String ("filter2"));
            expectEquals (doc.nodes[1].name, juce::String ("filter1"));

            const auto edges = dsl::audioEdges (doc);
            bool saw = false;
            for (const auto& e : edges)
                if (e.fromIndex == 0 && e.toIndex == 1)
                    saw = true;
            expect (saw);

            dsl::GraphNode mid;
            mid.type = "stage";
            mid.name = "stage1";
            mid.busName = "main";
            mid.args["y"] = "x";
            expect (dsl::insertOnEdge (doc, 0, 1, mid, error), error);
            expectEquals ((int) doc.nodes.size(), 3);
            expectEquals (doc.nodes[1].name, juce::String ("stage1"));
        }

        beginTest ("visualRail puts LFO on MOD and out on MAIN");
        {
            const juce::String script =
                "osc1: shape = sine; freq = a\n"
                "stage1: y = tube(x, a)\n"
                "bus shine:\n"
                "  send: in = 1\n"
                "  reverb1: size = b\n"
                "out: main = 1-c; shine = c\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expectEquals ((int) doc.nodes.size(), 6);
            expect (dsl::isModulator (doc.nodes[0]));
            expectEquals (dsl::visualRail (doc.nodes[0]), juce::String ("mod"));
            expectEquals (dsl::visualRail (doc.nodes[1]), juce::String ("main"));
            expectEquals (dsl::visualRail (doc.nodes[2]), juce::String ("shine"));
            expectEquals (dsl::visualRail (doc.nodes[3]), juce::String ("shine"));
            expectEquals (dsl::visualRail (doc.nodes[4]), juce::String ("shine"));
            expectEquals (dsl::visualRail (doc.nodes[5]), juce::String ("main"));

            dsl::assignNodeToBus (doc, 1, "mod");
            bool stageOnMain = false;
            for (const auto& n : doc.nodes)
                if (n.name == "stage1")
                {
                    expectEquals (n.busName, juce::String ("main"));
                    stageOnMain = true;
                }
            expect (stageOnMain);
        }

        beginTest ("assignNodeToBus moves a block onto a named bus and back");
        {
            const juce::String script =
                "stage1: y = x\n"
                "bus dirt:\n"
                "  send: in = 1\n"
                "  stage2: y = tube(x, a)\n"
                "out: main = 1; dirt = c\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);
            expectEquals ((int) doc.nodes.size(), 5);
            expectEquals (doc.nodes[0].name, juce::String ("stage1"));
            expectEquals (doc.nodes[0].busName, juce::String ("main"));

            dsl::assignNodeToBus (doc, 0, "dirt");
            int stage1 = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].name == "stage1")
                    stage1 = i;
            expect (stage1 >= 0);
            expectEquals (doc.nodes[(size_t) stage1].busName, juce::String ("dirt"));
            expect (stage1 > 0);

            const juce::String onBus = dsl::emit (doc);
            expect (onBus.contains ("bus dirt:"));
            expect (onBus.indexOf ("stage1:") > onBus.indexOf ("bus dirt:"));

            dsl::assignNodeToBus (doc, stage1, "main");
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].name == "stage1")
                    expectEquals (doc.nodes[(size_t) i].busName, juce::String ("main"));

            const juce::String onMain = dsl::emit (doc);
            expect (onMain.indexOf ("stage1:") < onMain.indexOf ("bus dirt:"));

            const int sendIdx = [&]
            {
                for (int i = 0; i < (int) doc.nodes.size(); ++i)
                    if (doc.nodes[(size_t) i].type == "send")
                        return i;
                return -1;
            }();
            expect (sendIdx >= 0);
            const auto before = dsl::emit (doc);
            dsl::assignNodeToBus (doc, sendIdx, "main");
            expectEquals (dsl::emit (doc), before);
        }

        beginTest ("factory presets parse/emit/parse semanticallyEqual");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)),
                        "factory_presets.json missing");

            const juce::String names[] = { "Mesa High Gain", "Side Delay", "OTT Smash" };
            for (const auto& name : names)
            {
                const auto* entry = lib.findByName (name);
                if (entry == nullptr)
                {
                    expect (false, name + " not present in FactoryPresetLibrary");
                    continue;
                }

                dsl::GraphDocument first, second;
                juce::String error;
                expect (dsl::parse (entry->script, first, error), name + " parse failed: " + error);
                const juce::String emitted = dsl::emit (first);
                expect (dsl::parse (emitted, second, error), name + " reparse failed: " + error);
                expect (dsl::semanticallyEqual (first, second),
                        name + " emit/parse is not semantically equal");
            }
        }

        beginTest ("mid-side and bus jacks split like rails");
        {
            const juce::String script =
                "ms1: mode = encode\n"
                "stage1: channel = mid; y = x\n"
                "stage2: channel = side; y = x\n"
                "ms2: mode = decode\n"
                "bus dirt:\n"
                "  send: in = 1\n"
                "out: main = 1; dirt = 0.3\n";
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);

            int enc = -1, dec = -1, mid = -1, side = -1, bus = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
            {
                const auto& n = doc.nodes[(size_t) i];
                if (dsl::isMsEncode (n)) enc = i;
                if (dsl::isMsDecode (n)) dec = i;
                if (n.name == "stage1") mid = i;
                if (n.name == "stage2") side = i;
                if (n.type == "bus") bus = i;
            }
            expect (enc >= 0 && dec >= 0 && mid >= 0 && side >= 0 && bus >= 0);
            expectEquals (dsl::visualRail (doc.nodes[(size_t) mid]), juce::String ("mid"));
            expectEquals (dsl::visualRail (doc.nodes[(size_t) side]), juce::String ("side"));
            expectEquals (dsl::visualRail (doc.nodes[(size_t) bus]), juce::String ("dirt"));

            const auto ej = dsl::jacksFor (doc.nodes[(size_t) enc], &doc);
            bool midOut = false, sideOut = false;
            for (const auto& j : ej)
            {
                if (j.id == "mid" && j.output) midOut = true;
                if (j.id == "side" && j.output) sideOut = true;
            }
            expect (midOut && sideOut);

            const auto bj = dsl::jacksFor (doc.nodes[(size_t) bus], &doc);
            expect ((int) bj.size() >= 2);

            const auto vis = dsl::visualAudioEdges (doc);
            bool encToMid = false, encToSide = false, midToDec = false, sideToDec = false;
            for (const auto& e : vis)
            {
                if (e.fromIndex == enc && e.toIndex == mid) encToMid = true;
                if (e.fromIndex == enc && e.toIndex == side) encToSide = true;
                if (e.fromIndex == mid && e.toIndex == dec) midToDec = true;
                if (e.fromIndex == side && e.toIndex == dec) sideToDec = true;
            }
            expect (encToMid && encToSide && midToDec && sideToDec);
        }

        beginTest ("jacksFor gives each mix bus and knob its own port");
        {
            const juce::String script =
                "osc1: shape = sine; freq = 2\n"
                "filter1: type = lowpass; cutoff = a\n"
                "stage1: y = tube(x, osc1)\n"
                "comp1: threshold = -18; ratio = 4; source = sidechain\n"
                "bus dirt:\n"
                "  send: in = 1\n"
                "  reverb1: size = 0.4\n"
                "out: main = 1; dirt = c\n";

            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (script, doc, error), error);

            const auto inJ = dsl::jacksForInput();
            expectEquals ((int) inJ.size(), 1);
            expect (inJ[0].output);
            expectEquals (inJ[0].id, juce::String ("out"));

            int filter = -1, stage = -1, osc = -1, comp = -1, out = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
            {
                if (doc.nodes[(size_t) i].name == "filter1") filter = i;
                if (doc.nodes[(size_t) i].name == "stage1") stage = i;
                if (doc.nodes[(size_t) i].name == "osc1") osc = i;
                if (doc.nodes[(size_t) i].name == "comp1") comp = i;
                if (doc.nodes[(size_t) i].type == "out") out = i;
            }
            expect (filter >= 0 && stage >= 0 && osc >= 0 && comp >= 0 && out >= 0);

            const auto fj = dsl::jacksFor (doc.nodes[(size_t) filter], &doc);
            bool sawIn = false, sawOut = false, sawKnobA = false;
            for (const auto& j : fj)
            {
                if (j.id == "in" && ! j.output) sawIn = true;
                if (j.id == "out" && j.output) sawOut = true;
                if (j.id == "knob:a" && ! j.output) sawKnobA = true;
            }
            expect (sawIn && sawOut && sawKnobA);

            const auto sj = dsl::jacksFor (doc.nodes[(size_t) stage], &doc);
            bool sawModIn = false;
            for (const auto& j : sj)
                if (j.id == "osc1" && ! j.output && j.kind == "mod")
                    sawModIn = true;
            expect (sawModIn);

            const auto oj = dsl::jacksFor (doc.nodes[(size_t) osc], &doc);
            expectEquals ((int) oj.size(), 1);
            expectEquals (oj[0].id, juce::String ("mod"));
            expect (oj[0].output);

            const auto cj = dsl::jacksFor (doc.nodes[(size_t) comp], &doc);
            bool sawSc = false;
            for (const auto& j : cj)
                if (j.id == "sc" && ! j.output)
                    sawSc = true;
            expect (sawSc);

            const auto outJ = dsl::jacksFor (doc.nodes[(size_t) out], &doc);
            bool sawMain = false, sawDirt = false;
            for (const auto& j : outJ)
            {
                expect (! j.output);
                if (j.id == "main") sawMain = true;
                if (j.id == "dirt") sawDirt = true;
            }
            expect (sawMain && sawDirt);
            expect ((int) outJ.size() >= 2);

            const auto edges = dsl::audioEdges (doc);
            bool mainLandsOnMain = false, dirtLandsOnDirt = false;
            for (const auto& e : edges)
            {
                if (e.toIndex == out && e.toJack == "main")
                    mainLandsOnMain = true;
                if (e.toIndex == out && e.toJack == "dirt")
                    dirtLandsOnDirt = true;
            }
            expect (mainLandsOnMain);
            expect (dirtLandsOnDirt);

            expect (dsl::connectJack (doc, filter, "out", out, "dirt", error), error);
            int filterNow = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].name == "filter1")
                    filterNow = i;
            expect (filterNow >= 0);
            expectEquals (doc.nodes[(size_t) filterNow].busName, juce::String ("dirt"));
        }

        beginTest ("rewriteParamDisplayName keeps range and collectParamDisplayNames reads it");
        {
            const juce::String script =
                "param a = Drive [0.0, 2.0]  # tone\n"
                "stage1: y = tanh(x * a)\n";
            const auto next = dsl::rewriteParamDisplayName (script, 0, "Crunch");
            expect (next.contains ("param a = Crunch [0.0, 2.0]"));
            expect (next.contains ("# tone"));
            expect (next.contains ("stage1:"));

            juce::String names[6];
            dsl::collectParamDisplayNames (next, names, 6);
            expectEquals (names[0], juce::String ("Crunch"));
            expect (names[1].isEmpty());

            const auto added = dsl::rewriteParamDisplayName ("stage1: y = x\n", 1, "Tone");
            expect (added.contains ("param b = Tone"));
            expect (added.contains ("stage1: y = x"));
        }
    }
};

#endif // GRAPHMODELTEST_H
