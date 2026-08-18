#pragma once

#include <JuceHeader.h>
#include "../src/bridge/HostSnapshot.h"
#include "../src/core/Config.h"
#include "../src/core/PluginProcessor.h"

class HostSnapshotTest : public juce::UnitTest
{
public:
    HostSnapshotTest() : juce::UnitTest ("HostSnapshot", "Bridge") {}

    void runTest() override
    {
        beginTest ("footer CPU is 0-100");
        {
            expectEquals (bridge::footerCpu (0.f), 0);
            expectEquals (bridge::footerCpu (0.42f), 42);
            expectEquals (bridge::footerCpu (1.73f), 100);
            expectEquals (bridge::footerCpu (Config::cpuDisplayPercent (1.73f) / 100.f), 100);
        }

        beginTest ("SAFE is a mode word not a percent");
        {
            expectEquals (juce::String (bridge::modeWord (true, false, false)), juce::String ("SAFE"));
            expectEquals (juce::String (bridge::modeWord (false, true, false)), juce::String ("BYPASS"));
            expectEquals (juce::String (bridge::modeWord (false, false, true)), juce::String ("LIVE"));
            expectEquals (juce::String (bridge::modeWord (false, false, false)), juce::String ("STUDIO"));
        }

        beginTest ("osFactorFromIndex maps 1/2/4/8");
        {
            expectEquals (bridge::osFactorFromIndex (0), 1);
            expectEquals (bridge::osFactorFromIndex (2), 4);
            expectEquals (bridge::osFactorFromIndex (3), 8);
        }

        beginTest ("paramGestureFromVar and presetCmdFromVar");
        {
            auto* p = new juce::DynamicObject();
            p->setProperty ("id", "a");
            p->setProperty ("value", 0.5);
            p->setProperty ("gesture", "begin");
            bridge::ParamGesture g;
            juce::String err;
            expect (bridge::paramGestureFromVar (juce::var (p), g, err), err);
            expectEquals (g.id, juce::String ("a"));
            expect (std::abs (g.value - 0.5f) < 1.0e-5f);
            expectEquals (g.phase, juce::String ("begin"));

            auto* pr = new juce::DynamicObject();
            pr->setProperty ("action", "next");
            bridge::PresetCmd c;
            expect (bridge::presetCmdFromVar (juce::var (pr), c, err), err);
            expectEquals (c.action, juce::String ("next"));

            expect (! bridge::paramGestureFromVar ("x", g, err));
            expect (! bridge::presetCmdFromVar (juce::var(), c, err));
        }

        beginTest ("choiceCmdFromVar accepts mode index for STUDIO/LIVE");
        {
            auto* p = new juce::DynamicObject();
            p->setProperty ("id", "mode");
            p->setProperty ("index", 1);
            bridge::ChoiceCmd c;
            juce::String err;
            expect (bridge::choiceCmdFromVar (juce::var (p), c, err), err);
            expectEquals (c.id, juce::String ("mode"));
            expectEquals (c.index, 1);
        }

        beginTest ("hostVar reports sidechainOn from the SC bus");
        {
            NeuroKoreAudioProcessor proc;
            const auto host = bridge::hostVar (proc);
            expect (host.hasProperty ("sidechainOn"));
            const bool on = (bool) host.getProperty ("sidechainOn", false);
            if (auto* bus = proc.getBus (true, 1))
                expectEquals (on, bus->isEnabled());
            else
                expect (! on);
        }
    }
};
