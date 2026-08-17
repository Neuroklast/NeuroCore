#pragma once

#include <JuceHeader.h>
#include "../src/core/Config.h"
#include "../src/dsl/GraphModel.h"
#include "../src/dsl/PcbRouter.h"
#include "../src/ui/GraphCanvasComponent.h"
#include "CircuitContracts.h"

/** Few named contracts. Failure prints the board dump, not 200 sample expects. */
class CircuitContractTest : public juce::UnitTest
{
public:
    CircuitContractTest() : juce::UnitTest ("CircuitContract", "UI") {}

    void runTest() override
    {
        beginTest ("CPU footer contract is 0-100");
        {
            expectEquals (Config::cpuDisplayPercent (0.f), 0);
            expectEquals (Config::cpuDisplayPercent (0.42f), 42);
            expectEquals (Config::cpuDisplayPercent (1.73f), 100);
        }

        beginTest ("fold hit contains the painted chevron");
        {
            const auto chev = GraphCanvasComponent::foldChevronRect (176, 64, false, 1.f);
            const auto hit = GraphCanvasComponent::foldHitRect (176, 64, false, 1.f);
            expect (hit.contains (chev.getCentre()), "chevron and hit must be the same control");
        }

        beginTest ("simple stage-filter chain flows IN left to OUT right");
        {
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse ("stage1: y = x\nfilter1: type = lowpass; cutoff = 800\n",
                                doc, err), err);
            const auto hint = dsl::tidyLayout (doc, 1400, 800);
            expect (CircuitContracts::simpleChainFlow (hint, doc),
                    CircuitContracts::dump (hint, doc));
            expect (CircuitContracts::noKnobJacks (doc));
            expect (CircuitContracts::snappedAndSeparate (doc),
                    CircuitContracts::dump (hint, doc));
        }

        beginTest ("bound knobs are not jacks");
        {
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse ("filter1: type = lowpass; cutoff = a; resonance = 0.5\n",
                                doc, err), err);
            expect (CircuitContracts::noKnobJacks (doc));
            expect (dsl::knobBindings (doc.nodes[0]).size() == 1);
        }

        beginTest ("same-row route has zero turns and beats a staircase");
        {
            dsl::PcbRouter r;
            const auto path = r.route ({ 16.f, 80.f }, { 240.f, 80.f }, {});
            expectEquals (dsl::PcbRouter::countTurns (path.waypoints), 0);
            std::vector<dsl::PcbPoint> stair {
                { 16.f, 80.f }, { 64.f, 80.f }, { 64.f, 128.f },
                { 160.f, 128.f }, { 160.f, 80.f }, { 240.f, 80.f }
            };
            expect (dsl::PcbRouter::pathCost (path.waypoints, 16.f)
                    < dsl::PcbRouter::pathCost (stair, 16.f));
        }

        beginTest ("Phaser Lab hard contracts + open IN/OUT (screenshot 231501)");
        {
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (
                "osc1: shape = sine; freq = a; depth = 1.0\n"
                "stage1: y = x\n"
                "bus swirl:\n"
                "  send: in = 1\n"
                "  filter1: type = highpass; cutoff = c + (0.5 + 0.5 * osc1) * (b * 0.22); resonance = f\n"
                "  filter2: type = lowpass; cutoff = c + b * 0.55 + (0.5 + 0.5 * osc1) * (b * 0.35); resonance = 0.5\n"
                "  stage2: y = softclip(x, d)\n"
                "  filter3: type = lowpass; cutoff = 12000; resonance = 0.28\n"
                "out: main = 1-e; swirl = e\n",
                doc, err), err);
            const auto hint = dsl::tidyLayout (doc, 1400, 800);
            const auto report = CircuitContracts::evaluate (hint, doc);
            logMessage ("board " + report.dump);
            for (const auto& c : report.hard)
                expect (c.pass, c.name + " " + report.dump);
            juce::String open;
            for (const auto& c : report.soft)
                if (! c.pass)
                    open << c.name << " ";
            if (open.isNotEmpty())
                logMessage ("OPEN circuit (do not tick STATUS): " + open + report.dump);
        }
    }
};
