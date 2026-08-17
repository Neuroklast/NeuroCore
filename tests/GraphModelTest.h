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

        beginTest ("octaver exposes sub up mix tone thresh and no knob jacks");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (
                "octaver1: sub = a; up = 0.1; mix = 0.5; tone = c; thresh = 0.05",
                doc, error), error);
            expectEquals ((int) doc.nodes.size(), 1);
            const auto keys = dsl::editableArgKeys (doc.nodes[0]);
            expect (keys.contains ("sub"));
            expect (keys.contains ("up"));
            expect (keys.contains ("mix"));
            expect (keys.contains ("tone"));
            expect (keys.contains ("thresh"));
            const auto jacks = dsl::jacksFor (doc.nodes[0], &doc);
            bool sawIn = false, sawOut = false, sawKnob = false;
            for (const auto& j : jacks)
            {
                if (j.id == "in" && ! j.output) sawIn = true;
                if (j.id == "out" && j.output) sawOut = true;
                if (j.kind == "knob") sawKnob = true;
            }
            expect (sawIn && sawOut);
            expect (! sawKnob);
            const auto binds = dsl::knobBindings (doc.nodes[0]);
            expectEquals ((int) binds.size(), 2);
        }

        beginTest ("noisegate parses as ngate with threshold attack release");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("ngate1: threshold = -48; attack = 0.002; release = 0.05",
                                doc, error), error);
            expectEquals ((int) doc.nodes.size(), 1);
            expectEquals (doc.nodes[0].type, juce::String ("noisegate"));
            expectEquals (doc.nodes[0].name, juce::String ("ngate1"));
            const auto keys = dsl::editableArgKeys (doc.nodes[0]);
            expect (keys.contains ("threshold"));
            expect (keys.contains ("attack"));
            expect (keys.contains ("release"));
            const juce::String emitted = dsl::emit (doc);
            expect (emitted.contains ("ngate1:"));
            expect (emitted.contains ("threshold = -48"));
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

            const auto vis = dsl::visualAudioEdges (doc);
            bool busIn = false, busOut = false;
            for (const auto& e : vis)
            {
                if (e.toIndex == 1 && e.fromIndex == -1)
                    busIn = true;
                if (e.fromIndex == 1 && e.toIndex == 2)
                    busOut = true;
            }
            expect (busIn, "bus header must take a send from IN");
            expect (busOut, "bus header must feed its send block");
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

        beginTest ("factory library has 500+ unique named jobs");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)),
                        "factory_presets.json missing");
            expect (lib.getEntries().size() >= 500);
            juce::StringArray seen;
            for (const auto& e : lib.getEntries())
            {
                expect (e.name.isNotEmpty());
                expect (! seen.contains (e.name), e.name + " duplicated");
                seen.add (e.name);
            }
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

        beginTest ("jacksFor gives each mix bus its own port, not knobs");
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
                if (j.kind == "knob") sawKnobA = true;
            }
            expect (sawIn && sawOut);
            expect (! sawKnobA);

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

        beginTest ("rewriteParamRange updates bounds and delay keys list every jack");
        {
            const juce::String script =
                "param a = Drive [0.0, 2.0]  # tone\n"
                "delay1: time = 140; feedback = 0.3; mix = 0.4\n";
            const auto next = dsl::rewriteParamRange (script, 0, 1.5f, 8.f);
            expect (next.contains ("param a = Drive [1.5, 8]"));
            expect (next.contains ("# tone"));
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            const auto keys = dsl::editableArgKeys (doc.nodes[0]);
            expect (keys.contains ("time"));
            expect (keys.contains ("sync"));
            expect (keys.contains ("feedback"));
            expect (keys.contains ("mix"));
            expect (keys.contains ("damp"));
            expect (keys.contains ("pingpong"));
        }

        beginTest ("tidyLayout places a chain left to right without reordering");
        {
            const juce::String script =
                "stage1: y = x\n"
                "filter1: type = lowpass; cutoff = 800\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            juce::StringArray names;
            for (const auto& n : doc.nodes)
                names.add (n.name);
            dsl::GraphDocument before = doc;
            dsl::tidyLayout (doc);
            expect (dsl::semanticallyEqual (before, doc));
            expect (dsl::hasAllPositions (doc));
            expectEquals (doc.nodes[0].name, names[0]);
            expectEquals (doc.nodes[1].name, names[1]);
            expect (doc.nodes[0].x < doc.nodes[1].x);
            const int g = dsl::kTidyGrid;
            expectEquals ((int) doc.nodes[0].x % g, 0);
            expectEquals ((int) doc.nodes[0].y % g, 0);
            expectEquals ((int) doc.nodes[1].x % g, 0);
            const float dx = std::abs (doc.nodes[0].x - doc.nodes[1].x);
            const float dy = std::abs (doc.nodes[0].y - doc.nodes[1].y);
            expect (dx >= (float) dsl::kTidyCardW || dy >= (float) dsl::kTidyCardH);
        }

        beginTest ("tidyLayout fits a short chain into the view when it can");
        {
            const juce::String script =
                "stage1: y = x\n"
                "filter1: type = lowpass; cutoff = 800\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            const auto hint = dsl::tidyLayout (doc, 1200, 420);
            expect (hint.fitted);
            expect (hint.boardW <= 1200.f);
            expect (hint.boardH <= 420.f);
            expect (hint.boardW > 0.f && hint.boardH > 0.f);
        }

        beginTest ("tidyLayout does not crush a graph that cannot fit");
        {
            juce::String script;
            for (int i = 0; i < 12; ++i)
                script << "stage" << (i + 1) << ": y = x\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            const auto hint = dsl::tidyLayout (doc, 200, 160);
            expect (! hint.fitted);
            expect (dsl::hasAllPositions (doc));
            expect (doc.nodes[0].x < doc.nodes[11].x);
            const float dx = std::abs (doc.nodes[0].x - doc.nodes[1].x);
            expect (dx >= (float) dsl::kTidyCardW);
        }

        beginTest ("tidyLayout wraps a single chain into rows when one line is too wide");
        {
            juce::String script;
            for (int i = 0; i < 8; ++i)
                script << "stage" << (i + 1) << ": y = x\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            const auto names = [&]
            {
                juce::StringArray a;
                for (const auto& n : doc.nodes)
                    a.add (n.name);
                return a;
            }();
            const auto hint = dsl::tidyLayout (doc, 800, 620);
            expect (hint.fitted, "wrap should fit 8-stage chain in 800x620");
            expect (hint.boardW <= 800.f);
            expect (hint.boardH <= 620.f);
            for (int i = 0; i < (int) names.size(); ++i)
                expectEquals (doc.nodes[(size_t) i].name, names[(size_t) i]);
            float minY = 1.0e9f, maxY = -1.0e9f;
            for (const auto& n : doc.nodes)
            {
                minY = juce::jmin (minY, n.y);
                maxY = juce::jmax (maxY, n.y);
            }
            expect (maxY - minY >= (float) dsl::kTidyCardH,
                    "wrapped chain must use more than one row");
        }

        beginTest ("tidyLayout wraps each rail so a bus smash fits the view");
        {
            const juce::String script =
                "stage1: y = x\n"
                "bus smash:\n"
                "  send: in = 1\n"
                "  stage2: y = tube(x, a)\n"
                "  delay1: time = 22; mix = 1\n"
                "  reverb1: size = 0.3; mix = 1\n"
                "  filter1: type = lowpass; cutoff = 4800\n"
                "  stage3: y = softclip(y, 1.08)\n"
                "out: main = 1-d; smash = d\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            juce::StringArray names;
            for (const auto& n : doc.nodes)
                names.add (n.name);
            const auto hint = dsl::tidyLayout (doc, 800, 620);
            expect (hint.fitted, "Trailer-style smash must wrap into 800x620");
            expect (hint.boardW <= 800.f);
            expect (hint.boardH <= 620.f);
            for (int i = 0; i < (int) names.size(); ++i)
                expectEquals (doc.nodes[(size_t) i].name, names[(size_t) i]);
            expect (dsl::semanticallyEqual ([&]
            {
                dsl::GraphDocument raw;
                juce::String e;
                dsl::parse (script, raw, e);
                return raw;
            }(), doc));
            float smashMinY = 1.0e9f, smashMaxY = -1.0e9f;
            float smashMaxX = -1.0e9f;
            int smashN = 0;
            for (const auto& n : doc.nodes)
            {
                if (n.type == "out" || dsl::visualRail (n) != "smash")
                    continue;
                ++smashN;
                smashMinY = juce::jmin (smashMinY, n.y);
                smashMaxY = juce::jmax (smashMaxY, n.y);
                smashMaxX = juce::jmax (smashMaxX, n.x);
            }
            expect (smashN >= 5);
            expect (smashMaxY - smashMinY >= (float) dsl::kTidyCardH,
                    "smash rail must wrap to more than one row");
            expect (smashMaxX + (float) dsl::kTidyCardW <= 800.f);
        }

        beginTest ("tidyLayout never overlaps chips and parks OUT to the right");
        {
            const juce::String script =
                "ms1: mode = encode\n"
                "stage1: channel = mid; y = tube(x, a)\n"
                "stage2: channel = side; y = x * b\n"
                "ms2: mode = decode\n"
                "filter1: type = lowpass; cutoff = e\n"
                "out: main = 1\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            dsl::tidyLayout (doc, 1400, 800);
            expect (dsl::hasAllPositions (doc));
            int outI = -1, lastMain = -1;
            float lastMainX = -1.0e9f;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
            {
                const auto& n = doc.nodes[(size_t) i];
                if (n.type == "out")
                    outI = i;
                else if (dsl::visualRail (n) == "main" && n.x >= lastMainX)
                {
                    lastMainX = n.x;
                    lastMain = i;
                }
            }
            expect (outI >= 0 && lastMain >= 0);
            expect (doc.nodes[(size_t) outI].x > lastMainX + 8.f, "OUT must sit right of the last main chip");
        }

        beginTest ("tidyLayout keeps OUT right of last main chip when the view is narrow");
        {
            juce::String script;
            for (int i = 0; i < 8; ++i)
                script << "stage" << (i + 1) << ": y = x\n";
            script << "out: main = 1\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            dsl::tidyLayout (doc, 800, 620);
            int outI = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].type == "out")
                    outI = i;
            expect (outI >= 0);
            const float outX = doc.nodes[(size_t) outI].x;
            const float outY = doc.nodes[(size_t) outI].y;
            float rowMaxX = -1.0e9f;
            for (const auto& n : doc.nodes)
            {
                if (n.type == "out" || ! std::isfinite (n.x) || dsl::visualRail (n) != "main")
                    continue;
                if (std::abs (n.y - outY) <= (float) dsl::kTidyCardH)
                    rowMaxX = juce::jmax (rowMaxX, n.x);
            }
            expect (outX > rowMaxX + 8.f,
                    "narrow view must not fold OUT onto the left margin");
            expect (outX > (float) dsl::kTidyMargin + 8.f);
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                for (int j = i + 1; j < (int) doc.nodes.size(); ++j)
                {
                    const auto& a = doc.nodes[(size_t) i];
                    const auto& b = doc.nodes[(size_t) j];
                    expect (! dsl::nodeRectsClash (a.x, a.y,
                                                   (float) dsl::tidyNodeWidth (a),
                                                   (float) dsl::tidyNodeHeight (a, &doc),
                                                   b.x, b.y,
                                                   (float) dsl::tidyNodeWidth (b),
                                                   (float) dsl::tidyNodeHeight (b, &doc),
                                                   (float) dsl::kTidyMinGap),
                            "chips must keep a gap and never overlap");
                }
        }

        beginTest ("separateOverlappingNodes pulls stacked chips apart");
        {
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse ("stage1: y = x\nstage2: y = x\n", doc, err), err);
            dsl::setPosition (doc, 0, 80.f, 80.f);
            dsl::setPosition (doc, 1, 80.f, 80.f);
            dsl::separateOverlappingNodes (doc, 16.f, 16.f);
            expect (! dsl::nodeRectsClash (doc.nodes[0].x, doc.nodes[0].y,
                                           (float) dsl::tidyNodeWidth (doc.nodes[0]),
                                           (float) dsl::tidyNodeHeight (doc.nodes[0], &doc),
                                           doc.nodes[1].x, doc.nodes[1].y,
                                           (float) dsl::tidyNodeWidth (doc.nodes[1]),
                                           (float) dsl::tidyNodeHeight (doc.nodes[1], &doc),
                                           (float) dsl::kTidyMinGap));
        }

        beginTest ("tidyLayout puts mid/side off the main rail");
        {
            const juce::String script =
                "ms1: mode = encode\n"
                "stage1: channel = mid; y = x\n"
                "stage2: channel = side; y = x * 0.8\n"
                "ms2: mode = decode\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            juce::StringArray names;
            for (const auto& n : doc.nodes)
                names.add (n.name);
            dsl::tidyLayout (doc);
            for (int i = 0; i < (int) names.size(); ++i)
                expectEquals (doc.nodes[(size_t) i].name, names[(size_t) i]);
            int mid = -1, side = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
            {
                if (doc.nodes[(size_t) i].name == "stage1") mid = i;
                if (doc.nodes[(size_t) i].name == "stage2") side = i;
            }
            expect (mid >= 0 && side >= 0);
            expect (std::abs (doc.nodes[(size_t) mid].y - doc.nodes[(size_t) side].y)
                    >= (float) dsl::kTidyCardH * 0.5f);
        }

        beginTest ("named-bus mid/side stay on the bus rail (Wide Canvas)");
        {
            const juce::String script =
                "stage1: y = x\n"
                "ms1: mode = encode\n"
                "stage2: channel = mid; y = x\n"
                "ms2: mode = decode\n"
                "bus sides:\n"
                "  send: in = 1\n"
                "  ms3: mode = encode\n"
                "  stage3: channel = mid; y = x * 0.0\n"
                "  stage4: channel = side; y = x\n"
                "  ms4: mode = decode\n"
                "  reverb1: size = 0.5; mix = 1\n"
                "out: main = 0.7; sides = 0.3\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            auto railOf = [&] (const juce::String& name) -> juce::String
            {
                for (const auto& n : doc.nodes)
                    if (n.name == name)
                        return dsl::visualRail (n);
                return {};
            };
            expectEquals (railOf ("stage2"), juce::String ("mid"));
            expectEquals (railOf ("stage3"), juce::String ("sides"));
            expectEquals (railOf ("stage4"), juce::String ("sides"));
            expectEquals (railOf ("reverb1"), juce::String ("sides"));
            dsl::tidyLayout (doc, 1100, 640);
            expect (dsl::hasAllPositions (doc));
            float midY = 0.f, sidesY = 0.f;
            int sidesN = 0;
            float sidesMinY = 1.0e9f, sidesMaxY = -1.0e9f;
            for (const auto& n : doc.nodes)
            {
                if (n.name == "stage2")
                    midY = n.y;
                if (dsl::visualRail (n) == "sides")
                {
                    ++sidesN;
                    sidesMinY = juce::jmin (sidesMinY, n.y);
                    sidesMaxY = juce::jmax (sidesMaxY, n.y);
                    sidesY += n.y;
                }
            }
            expect (sidesN >= 4);
            sidesY /= (float) sidesN;
            expect (std::abs (midY - sidesY) >= (float) dsl::kTidyCardH * 0.4f,
                    "sides bus must not sit on the main mid row");
            expect (sidesMaxY - sidesMinY < (float) dsl::kTidyRowGap * 8.f + (float) dsl::kTidyCardH * 4.f);
        }

        beginTest ("OUT edit keys include every mix jack including xover bands");
        {
            const juce::String script =
                "xover1: f1 = 200; f2 = 2500\n"
                "out: main = 1-c; mid = c\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            int out = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].type == "out")
                    out = i;
            expect (out >= 0);
            const auto jacks = dsl::jacksFor (doc.nodes[(size_t) out], &doc);
            auto hasJack = [&] (const juce::String& id)
            {
                for (const auto& j : jacks)
                    if (j.id == id && ! j.output)
                        return true;
                return false;
            };
            expect (hasJack ("main"));
            expect (hasJack ("mid"));
            expect (hasJack ("low"));
            expect (hasJack ("high"));
            const auto keys = dsl::editableArgKeys (doc.nodes[(size_t) out], &doc);
            expect (keys.contains ("main"));
            expect (keys.contains ("mid"));
            expect (keys.contains ("low"));
            expect (keys.contains ("high"));
        }

        beginTest ("parked node is not in the serial audio chain");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("filter1: type = lowpass; cutoff = 800\n"
                                "stage1: y = x\n"
                                "out: main = 1\n", doc, error), error);
            expectEquals ((int) doc.nodes.size(), 3);
            auto edges = dsl::audioEdges (doc);
            bool stageIn = false;
            for (const auto& e : edges)
                if (e.fromIndex >= 0 && doc.nodes[(size_t) e.fromIndex].name == "stage1")
                    stageIn = true;
            expect (stageIn);

            int stageIdx = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].name == "stage1")
                    stageIdx = i;
            dsl::parkNode (doc, stageIdx);
            expect (dsl::isParked (doc.nodes[(size_t) [&]
            {
                for (int i = 0; i < (int) doc.nodes.size(); ++i)
                    if (doc.nodes[(size_t) i].name == "stage1")
                        return i;
                return 0;
            }()]));

            const auto parked = dsl::emit (doc);
            expect (parked.contains ("bus __park:"));
            edges = dsl::audioEdges (doc);
            stageIn = false;
            bool filterToOut = false;
            for (const auto& e : edges)
            {
                if (e.fromIndex >= 0 && doc.nodes[(size_t) e.fromIndex].name == "stage1")
                    stageIn = true;
                if (e.fromIndex >= 0 && e.toIndex >= 0
                    && doc.nodes[(size_t) e.fromIndex].name == "filter1"
                    && doc.nodes[(size_t) e.toIndex].type == "out")
                    filterToOut = true;
            }
            expect (! stageIn, "parked chip must not sit on the audio path");
            expect (filterToOut);

            dsl::GraphDocument again;
            expect (dsl::parse (parked, again, error), error);
            bool stillParked = false;
            for (const auto& n : again.nodes)
                if (n.name == "stage1")
                    stillParked = dsl::isParked (n);
            expect (stillParked);

            int filter = -1, stage = -1;
            for (int i = 0; i < (int) again.nodes.size(); ++i)
            {
                if (again.nodes[(size_t) i].name == "filter1") filter = i;
                if (again.nodes[(size_t) i].name == "stage1") stage = i;
            }
            expect (dsl::connectAudio (again, filter, stage, error), error);
            for (const auto& n : again.nodes)
                if (n.name == "stage1")
                    expect (! dsl::isParked (n));
            expect (! dsl::emit (again).contains ("bus __park:"));
        }

        beginTest ("unplug last chip from OUT parks it");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("filter1: type = lowpass; cutoff = 800\n"
                                "stage1: y = x\n"
                                "out: main = 1\n", doc, error), error);
            int stage = -1, out = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
            {
                if (doc.nodes[(size_t) i].name == "stage1") stage = i;
                if (doc.nodes[(size_t) i].type == "out") out = i;
            }
            expect (dsl::disconnectAudio (doc, stage, out, error), error);
            bool parked = false;
            for (const auto& n : doc.nodes)
                if (n.name == "stage1")
                    parked = dsl::isParked (n);
            expect (parked);
        }

        beginTest ("widen and ott expose every editable key");
        {
            dsl::GraphDocument w, o;
            juce::String err;
            expect (dsl::parse ("widen1: width = 0.6; delay = 12; bass = 180", w, err), err);
            const auto wk = dsl::editableArgKeys (w.nodes[0]);
            expect (wk.contains ("width"));
            expect (wk.contains ("delay"));
            expect (wk.contains ("bass"));
            expect (dsl::parse ("ott1: depth = 0.5; time = 0.3; f1 = 90; f2 = 3200", o, err), err);
            const auto ok = dsl::editableArgKeys (o.nodes[0]);
            expect (ok.contains ("depth"));
            expect (ok.contains ("f1"));
            expect (ok.contains ("f2"));
        }

        beginTest ("osc with two destinations gets two mod output jacks");
        {
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (
                "osc1: shape = sine; freq = 0.4\n"
                "filter1: type = lowpass; cutoff = 800 + osc1 * 400\n"
                "filter2: type = highpass; cutoff = 200 + osc1 * 80\n",
                doc, err), err);
            int osc = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
                if (doc.nodes[(size_t) i].name == "osc1")
                    osc = i;
            expect (osc >= 0);
            const auto jacks = dsl::jacksFor (doc.nodes[(size_t) osc], &doc);
            int outs = 0;
            for (const auto& j : jacks)
                if (j.output && j.kind == "mod")
                    ++outs;
            expectEquals (outs, 2);
        }

        beginTest ("tidyLayout puts IN top-left and OUT bottom-right");
        {
            const juce::String script =
                "osc1: shape = sine; freq = 0.4\n"
                "stage1: y = x\n"
                "filter1: type = lowpass; cutoff = 800 + osc1 * 200\n"
                "out: main = 1\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            const auto hint = dsl::tidyLayout (doc, 1400, 800);
            expect (hint.inX <= (float) dsl::kTidyMargin + 0.5f);
            expect (hint.inY <= (float) dsl::kTidyMargin + 0.5f);
            int outI = -1, oscI = -1;
            for (int i = 0; i < (int) doc.nodes.size(); ++i)
            {
                if (doc.nodes[(size_t) i].type == "out") outI = i;
                if (doc.nodes[(size_t) i].name == "osc1") oscI = i;
            }
            expect (outI >= 0 && oscI >= 0);
            expect (doc.nodes[(size_t) outI].x >= hint.inX + (float) dsl::kTidyCardW);
            expect (doc.nodes[(size_t) outI].y >= hint.inY - 0.5f);
            expect (doc.nodes[(size_t) oscI].y >= doc.nodes[1].y - 0.5f,
                    "mod row stays at or below the main chain so IN stays top-left");
        }
    }
};

#endif // GRAPHMODELTEST_H
