#pragma once

#include <JuceHeader.h>
#include "../src/bridge/AstJson.h"
#include "../src/bridge/GraphOps.h"
#include "../src/bridge/TelemetryFrame.h"
#include "../src/bridge/UiReadyLatch.h"
#include "../src/dsl/GraphModel.h"
#include "../src/utils/FactoryPresetLibrary.h"

#ifndef NEUROKORE_RESOURCES_DIR
#define NEUROKORE_RESOURCES_DIR "resources"
#endif

/** WP0 contracts: AST JSON, graphOp, UI_READY latch, telemetry layout. */
class AstJsonTest : public juce::UnitTest
{
public:
    AstJsonTest() : juce::UnitTest ("AstJson", "Bridge") {}

    void runTest() override
    {
        beginTest ("simple chain parse -> toJson -> fromJson is semanticallyEqual");
        {
            const juce::String script =
                "# lead\n"
                "param a = Drive [0.0, 2.0]\n"
                "filter1: type = lowpass; cutoff = 800  # tone @16.0,32.0\n"
                "stage1: y = tanh(x * a)\n";

            dsl::GraphDocument first;
            juce::String error;
            expect (dsl::parse (script, first, error), error);

            juce::String jsonError;
            const auto json = dsl::toJson (first);
            expect (json.contains ("\"version\""));
            expect (json.contains ("\"nodes\""));

            dsl::GraphDocument back;
            expect (dsl::fromJson (json, back, jsonError), jsonError);
            expect (dsl::semanticallyEqual (first, back),
                    "JSON roundtrip lost type/name/args/params");
            expectEquals ((int) back.leadingComments.size(), 1);
            expect (back.leadingComments[0].contains ("lead"));
            expectEquals (back.nodes[0].trailingComment, juce::String ("tone"));
            expect (std::isfinite (back.nodes[0].x) && std::abs (back.nodes[0].x - 16.f) < 0.01f);
            expect (std::isfinite (back.nodes[0].y) && std::abs (back.nodes[0].y - 32.f) < 0.01f);
            expect (json.contains ("\"edges\""));
            expect (json.contains ("\"jacks\""));
            expect (! json.contains ("\"kind\":\"knob\""));
        }

        beginTest ("custom block parses as a formula chip");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("custom1: y = tanh(x * a); in2 = 0\n", doc, error), error);
            expectEquals (doc.nodes[0].type, juce::String ("custom"));
            expectEquals (doc.nodes[0].args.at ("y"), juce::String ("tanh(x * a)"));
            expectEquals (doc.nodes[0].args.at ("in2"), juce::String ("0"));
        }

        beginTest ("toJson emits mid/side cables and implicit OUT");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (
                        "ms1: mode = encode\n"
                        "stage1: channel = mid; y = x\n"
                        "stage2: channel = side; y = x\n"
                        "ms2: mode = decode\n",
                        doc, error),
                    error);
            const auto json = dsl::toJson (doc);
            expect (json.contains ("\"fromJack\":\"mid\""), json);
            expect (json.contains ("\"fromJack\":\"side\""), json);
            expect (json.contains ("\"to\":\"OUT\""), json);
        }

        beginTest ("lone MS encode still emits a mid cable, not a missing out");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("ms1: mode = encode\nstage1: y = x\n", doc, error), error);
            const auto json = dsl::toJson (doc);
            expect (json.contains ("\"fromJack\":\"mid\""), json);
            const auto edges = dsl::visualAudioEdges (doc);
            bool mid = false, deadOut = false;
            for (const auto& e : edges)
            {
                if (e.fromIndex < 0 || e.fromIndex >= (int) doc.nodes.size())
                    continue;
                if (doc.nodes[(size_t) e.fromIndex].name != "ms1")
                    continue;
                if (e.fromJack == "mid") mid = true;
                if (e.fromJack == "out") deadOut = true;
            }
            expect (mid, json);
            expect (! deadOut, json);
        }

        beginTest ("untagged chip between MS encode/decode is both rails, not a missing cable");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (
                        "ms1: mode = encode\n"
                        "reverb1: size = 0.5; decay = 1.4; mix = 0.3\n"
                        "ms2: mode = decode\n",
                        doc, error),
                    error);

            auto nameOf = [&] (int i) -> juce::String
            {
                if (i < 0) return "IN";
                if (i >= (int) doc.nodes.size()) return "OUT";
                return doc.nodes[(size_t) i].name;
            };
            auto hasEdge = [&] (const juce::String& from, const juce::String& fromJack,
                                const juce::String& to, const juce::String& toJack) -> bool
            {
                for (const auto& e : dsl::visualAudioEdges (doc))
                    if (nameOf (e.fromIndex) == from && e.fromJack == fromJack
                        && nameOf (e.toIndex) == to && e.toJack == toJack)
                        return true;
                return false;
            };

            const auto json = dsl::toJson (doc);
            expect (hasEdge ("ms1", "mid", "reverb1", "in"), json);
            expect (hasEdge ("ms1", "side", "reverb1", "in"), json);
            expect (hasEdge ("reverb1", "out", "ms2", "mid"), json);
            expect (hasEdge ("reverb1", "out", "ms2", "side"), json);
            expect (! hasEdge ("ms1", "out", "reverb1", "in"), json);
            bool encodeOut = false, decodeIn = false;
            for (const auto& e : dsl::visualAudioEdges (doc))
            {
                if (nameOf (e.fromIndex) == "ms1" && e.fromJack == "out") encodeOut = true;
                if (nameOf (e.toIndex) == "ms2" && e.toJack == "in") decodeIn = true;
            }
            expect (! encodeOut, json);
            expect (! decodeIn, json);
        }

        beginTest ("parser accepts mode = split / join as encode / decode aliases");
        {
            dsl::GraphDocument splitDoc, joinDoc, encDoc;
            juce::String error;
            expect (dsl::parse ("ms1: mode = split\nms2: mode = join\n", splitDoc, error), error);
            expect (dsl::isMsEncode (splitDoc.nodes[0]), "split should alias encode");
            expect (dsl::isMsDecode (splitDoc.nodes[1]), "join should alias decode");
            expect (dsl::parse ("ms1: mode = encode\n", encDoc, error), error);
            expect (dsl::isMsEncode (encDoc.nodes[0]), "factory encode stays Split");

            const auto splitJ = dsl::jacksFor (splitDoc.nodes[0], &splitDoc);
            const auto joinJ = dsl::jacksFor (splitDoc.nodes[1], &splitDoc);
            bool midOut = false, joinMidIn = false, splitOut = false;
            for (const auto& j : splitJ)
            {
                if (j.id == "mid" && j.output) midOut = true;
                if (j.id == "out") splitOut = true;
            }
            for (const auto& j : joinJ)
                if (j.id == "mid" && ! j.output) joinMidIn = true;
            expect (midOut && joinMidIn && ! splitOut);
        }

        beginTest ("L/R split has left/right outs; Join L/R cannot take MS rails");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse (
                        "ms1: mode = split; family = lr\n"
                        "stage1: y = x\n"
                        "ms2: mode = join; family = lr\n",
                        doc, error),
                    error);
            expectEquals ((int) doc.nodes.size(), 3);
            const auto splitJ = dsl::jacksFor (doc.nodes[0], &doc);
            const auto joinJ = dsl::jacksFor (doc.nodes[2], &doc);
            bool leftOut = false, rightOut = false, leftIn = false, rightIn = false;
            for (const auto& j : splitJ)
            {
                if (j.id == "left" && j.output) leftOut = true;
                if (j.id == "right" && j.output) rightOut = true;
            }
            for (const auto& j : joinJ)
            {
                if (j.id == "left" && ! j.output) leftIn = true;
                if (j.id == "right" && ! j.output) rightIn = true;
            }
            expect (leftOut && rightOut && leftIn && rightIn);

            auto nameOf = [&] (int i) -> juce::String
            {
                if (i < 0) return "IN";
                if (i >= (int) doc.nodes.size()) return "OUT";
                return doc.nodes[(size_t) i].name;
            };
            auto hasEdge = [&] (const juce::String& from, const juce::String& fromJack,
                                const juce::String& to, const juce::String& toJack) -> bool
            {
                for (const auto& e : dsl::visualAudioEdges (doc))
                    if (nameOf (e.fromIndex) == from && e.fromJack == fromJack
                        && nameOf (e.toIndex) == to && e.toJack == toJack)
                        return true;
                return false;
            };
            const auto json = dsl::toJson (doc);
            expect (hasEdge ("ms1", "left", "stage1", "in"), json);
            expect (hasEdge ("ms1", "right", "stage1", "in"), json);
            expect (hasEdge ("stage1", "out", "ms2", "left"), json);
            expect (hasEdge ("stage1", "out", "ms2", "right"), json);

            dsl::GraphDocument mixed;
            expect (dsl::parse (
                        "ms1: mode = split\n"
                        "ms2: mode = join; family = lr\n",
                        mixed, error),
                    error);
            expect (! dsl::connectJack (mixed, 0, "mid", 1, "left", error),
                    "Split MS must not connect to Join L/R");
            expect (error.isNotEmpty());
        }

        beginTest ("fromJson rejects garbage and leaves dest empty");
        {
            dsl::GraphDocument dest;
            dest.nodes.push_back ({});
            juce::String error;
            expect (! dsl::fromJson ("not-json", dest, error));
            expect (error.isNotEmpty());
            expect (dest.nodes.empty());
        }

        beginTest ("every factory script survives JSON roundtrip");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)),
                        "factory_presets.json missing");

            int checked = 0;
            for (const auto& entry : lib.getEntries())
            {
                dsl::GraphDocument first, back;
                juce::String parseErr, jsonErr;
                if (! dsl::parse (entry.script, first, parseErr))
                {
                    expect (false, entry.name + " parse failed: " + parseErr);
                    continue;
                }
                const auto json = dsl::toJson (first);
                expect (dsl::fromJson (json, back, jsonErr),
                        entry.name + " fromJson: " + jsonErr);
                expect (dsl::semanticallyEqual (first, back),
                        entry.name + " JSON roundtrip not semanticallyEqual");
                ++checked;
            }
            expect (checked >= 200, "expected 200+ factory scripts, got " + juce::String (checked));
        }

        beginTest ("graphOp park removes a chip from the serial audio chain");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("filter1: type = lowpass; cutoff = 800\n"
                                "stage1: y = x\n"
                                "out: main = 1\n", doc, error), error);

            expect (bridge::applyGraphOp (doc, bridge::GraphOp::park ("stage1"), error), error);
            bool parked = false;
            for (const auto& n : doc.nodes)
                if (n.name == "stage1")
                    parked = dsl::isParked (n);
            expect (parked, "stage1 should sit on __park");
            expect (dsl::emit (doc).contains ("bus __park:"));
        }

        beginTest ("graphOp connect uses GraphModel connectJack");
        {
            dsl::GraphDocument doc;
            juce::String error;
            expect (dsl::parse ("stage1: y = x\nfilter1: type = lowpass; cutoff = 800\n",
                                doc, error), error);
            expect (bridge::applyGraphOp (doc,
                                         bridge::GraphOp::connect ("stage1", "out", "filter1", "in"),
                                         error),
                    error);
            const auto edges = dsl::audioEdges (doc);
            bool linked = false;
            for (const auto& e : edges)
            {
                if (e.fromIndex < 0 || e.toIndex < 0)
                    continue;
                if (doc.nodes[(size_t) e.fromIndex].name == "stage1"
                    && doc.nodes[(size_t) e.toIndex].name == "filter1")
                    linked = true;
            }
            expect (linked, "connect should place stage1 before filter1");
        }

        beginTest ("graphOpFromVar connect and applyPositions");
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("op", "connect");
            obj->setProperty ("from", "stage1");
            obj->setProperty ("fromJack", "out");
            obj->setProperty ("to", "filter1");
            obj->setProperty ("toJack", "in");
            bridge::GraphOp op;
            juce::String error;
            expect (bridge::graphOpFromVar (juce::var (obj), op, error), error);
            expect (op.kind == bridge::GraphOp::Kind::Connect);

            dsl::GraphDocument doc;
            expect (dsl::parse ("stage1: y = x\nfilter1: type = lowpass; cutoff = 800\n", doc, error), error);
            auto* pos = new juce::DynamicObject();
            auto* xy = new juce::DynamicObject();
            xy->setProperty ("x", 48.0);
            xy->setProperty ("y", 32.0);
            pos->setProperty ("stage1", juce::var (xy));
            expect (bridge::applyPositions (doc, juce::var (pos), error), error);
            expect (std::abs (doc.nodes[0].x - 48.f) < 0.01f);
        }

        beginTest ("UI_READY latch blocks outbound until marked");
        {
            bridge::UiReadyLatch latch;
            expect (! latch.isReady());
            expect (! latch.allowOutbound());
            latch.markReady();
            expect (latch.isReady());
            expect (latch.allowOutbound());
        }

        beginTest ("telemetry frame magic/version/sizes");
        {
            const float scopeIn[]  { 0.1f, 0.2f };
            const float scopeOut[] { 0.3f, 0.4f };
            const float gx[] { 0.5f };
            const float gy[] { -0.5f };
            bridge::TelemetryDesc desc;
            desc.inPeak = 0.8f;
            desc.outPeak = 0.4f;
            desc.inRms = 0.2f;
            desc.outRms = 0.1f;
            desc.cpu01 = 0.5f;
            desc.scopeN = 2;
            desc.gonioN = 1;

            std::vector<std::uint8_t> buf (bridge::telemetryByteSize (desc));
            const auto written = bridge::writeTelemetryFrame (buf.data(), buf.size(), desc,
                                                              scopeIn, scopeOut, gx, gy);
            expectEquals ((int) written, (int) buf.size());
            expect (written >= bridge::kTelemetryHeaderBytes);

            bridge::TelemetryDesc read {};
            expect (bridge::readTelemetryHeader (buf.data(), buf.size(), read));
            expectEquals ((int) read.scopeN, 2);
            expectEquals ((int) read.gonioN, 1);
            expect (std::abs (read.inPeak - 0.8f) < 1.0e-6f);
            expect (std::abs (read.cpu01 - 0.5f) < 1.0e-6f);

            const auto* words = reinterpret_cast<const std::uint32_t*> (buf.data());
            expectEquals ((int) words[0], (int) bridge::kTelemetryMagic);
        }
    }
};
