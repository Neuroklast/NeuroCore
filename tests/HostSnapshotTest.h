#pragma once

#include <JuceHeader.h>
#include "../src/bridge/HostSnapshot.h"
#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "../src/utils/PresetLibrary.h"
#include "../src/utils/UiSettings.h"

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

        beginTest ("hostVar carries shared UI prefs");
        {
            UiSettings::get().setThemeId ("gold");
            UiSettings::get().setFrameRate (30);
            UiSettings::get().setDiscardPrompt (false);
            UiSettings::get().setCableWaveform (false);
            UiSettings::get().setMotion (CyberMotion::Reduced);
            NeuroKoreAudioProcessor proc;
            const auto host = bridge::hostVar (proc);
            expectEquals (host.getProperty ("theme", "").toString(), juce::String ("gold"));
            UiSettings::get().setThemeId ("digicide");
            const auto hostDigi = bridge::hostVar (proc);
            expectEquals (hostDigi.getProperty ("theme", "").toString(), juce::String ("digicide"));
            expectEquals ((int) host.getProperty ("frameRate", 0), 30);
            expectEquals ((int) host.getProperty ("discardPrompt", true), 0);
            expectEquals (host.getProperty ("cables", "").toString(), juce::String ("dots"));
            expectEquals (host.getProperty ("motion", "").toString(), juce::String ("reduced"));
            UiSettings::get().setThemeId ("signal");
            UiSettings::get().setFrameRate (60);
            UiSettings::get().setDiscardPrompt (true);
            UiSettings::get().setMotion (CyberMotion::Full);
        }

        beginTest ("shared UI prefs persist to AppData and hostVar on every instance");
        {
            UiSettings::get().setThemeId ("azure");
            UiSettings::get().setFrameRate (30);
            const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                               .getChildFile ("NEUROKLAST")
                               .getChildFile (Config::kAppDataFolder)
                               .getChildFile ("ui.settings");
            expect (f.existsAsFile(), f.getFullPathName());
            const auto xml = f.loadFileAsString();
            expect (xml.containsIgnoreCase ("azure"), xml);
            expect (xml.contains ("30"), xml);

            NeuroKoreAudioProcessor a;
            NeuroKoreAudioProcessor b;
            expectEquals (bridge::hostVar (a).getProperty ("theme", "").toString(),
                          juce::String ("azure"));
            expectEquals (bridge::hostVar (b).getProperty ("theme", "").toString(),
                          juce::String ("azure"));
            expectEquals ((int) bridge::hostVar (b).getProperty ("frameRate", 0), 30);

            UiSettings::get().setThemeId ("signal");
            UiSettings::get().setFrameRate (60);
        }

        beginTest ("window size, oversampling and polisher persist in ui.settings");
        {
            UiSettings::get().setEditorSize (1400, 941);
            UiSettings::get().setOversamplingIndex (3);
            UiSettings::get().setPolisherIndex (1);
            const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                               .getChildFile ("NEUROKLAST")
                               .getChildFile (Config::kAppDataFolder)
                               .getChildFile ("ui.settings");
            const auto xml = f.loadFileAsString();
            expect (xml.contains ("1400"), xml);
            expect (xml.contains ("oversamplingIndex"), xml);
            expect (xml.contains ("polisherIndex"), xml);
            expectEquals (UiSettings::get().editorWidth(), 1400);
            expectEquals (UiSettings::get().oversamplingIndex(), 3);
            expectEquals (UiSettings::get().polisherIndex(), 1);
            NeuroKoreAudioProcessor proc;
            if (auto* os = dynamic_cast<juce::AudioParameterChoice*> (
                    proc.apvts.getParameter (EffectParameters::oversampling)))
                expectEquals (os->getIndex(), 3);
            if (auto* po = dynamic_cast<juce::AudioParameterChoice*> (
                    proc.apvts.getParameter (EffectParameters::polisherMode)))
                expectEquals (po->getIndex(), 1);
            UiSettings::get().setOversamplingIndex (2);
            UiSettings::get().setPolisherIndex (0);
            UiSettings::get().setEditorSize (1280, 860);
        }

        beginTest ("host state restore does not overwrite global oversampling or polisher");
        {
            UiSettings::get().setOversamplingIndex (0);
            UiSettings::get().setPolisherIndex (0);
            NeuroKoreAudioProcessor stale;
            if (auto* mix = stale.apvts.getParameter (EffectParameters::dryWet))
                mix->setValueNotifyingHost (0.25f);
            juce::MemoryBlock blob;
            stale.getStateInformation (blob);

            UiSettings::get().setOversamplingIndex (3);
            UiSettings::get().setPolisherIndex (1);
            NeuroKoreAudioProcessor live;
            live.setStateInformation (blob.getData(), (int) blob.getSize());

            if (auto* os = dynamic_cast<juce::AudioParameterChoice*> (
                    live.apvts.getParameter (EffectParameters::oversampling)))
                expectEquals (os->getIndex(), 3);
            if (auto* po = dynamic_cast<juce::AudioParameterChoice*> (
                    live.apvts.getParameter (EffectParameters::polisherMode)))
                expectEquals (po->getIndex(), 1);
            expectEquals (UiSettings::get().oversamplingIndex(), 3);
            expectEquals (UiSettings::get().polisherIndex(), 1);
            if (auto* mix = live.apvts.getParameter (EffectParameters::dryWet))
                expectWithinAbsoluteError (mix->getValue(), 0.25f, 1.0e-4f);

            UiSettings::get().setOversamplingIndex (2);
            UiSettings::get().setPolisherIndex (0);
        }

        beginTest ("Unit meter prefs persist in ui.settings and hostVar");
        {
            UiSettings::get().setScopeSource ("out");
            UiSettings::get().setScopeX ("freq");
            UiSettings::get().setScopeY ("db");
            UiSettings::get().setScopeGrid (false);
            UiSettings::get().setScopeInvertY (true);
            UiSettings::get().setScopeDelta (true);
            const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                               .getChildFile ("NEUROKLAST")
                               .getChildFile (Config::kAppDataFolder)
                               .getChildFile ("ui.settings");
            const auto xml = f.loadFileAsString();
            expect (xml.contains ("scopeSource"), xml);
            expect (xml.containsIgnoreCase ("out"), xml);
            expect (xml.contains ("scopeX"), xml);
            expect (xml.contains ("freq"), xml);
            NeuroKoreAudioProcessor proc;
            const auto host = bridge::hostVar (proc);
            expectEquals (host.getProperty ("scopeSource", "").toString(), juce::String ("out"));
            expectEquals (host.getProperty ("scopeX", "").toString(), juce::String ("freq"));
            expectEquals (host.getProperty ("scopeY", "").toString(), juce::String ("db"));
            expectEquals ((int) host.getProperty ("scopeGrid", true), 0);
            expectEquals ((int) host.getProperty ("scopeInvertY", false), 1);
            expectEquals ((int) host.getProperty ("scopeDelta", false), 1);
            UiSettings::get().setScopeSource ("both");
            UiSettings::get().setScopeX ("samples");
            UiSettings::get().setScopeY ("linear");
            UiSettings::get().setScopeGrid (true);
            UiSettings::get().setScopeInvertY (false);
            UiSettings::get().setScopeDelta (false);
        }

        beginTest ("shared UI prefs reload from AppData written by another process");
        {
            UiSettings::get().setThemeId ("signal");
            UiSettings::get().setFrameRate (60);
            const auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                               .getChildFile ("NEUROKLAST")
                               .getChildFile (Config::kAppDataFolder)
                               .getChildFile ("ui.settings");
            expect (f.existsAsFile(), f.getFullPathName());
            auto xml = f.loadFileAsString();
            xml = xml.replace ("signal", "gold");
            xml = xml.replace (">60<", ">30<");
            xml = xml.replace ("val=\"60\"", "val=\"30\"");
            expect (f.replaceWithText (xml));
            expect (UiSettings::get().reloadFromDisk());
            expectEquals (UiSettings::get().themeId(), juce::String ("gold"));
            expectEquals (UiSettings::get().frameRate(), 30);
            NeuroKoreAudioProcessor other;
            expectEquals (bridge::hostVar (other).getProperty ("theme", "").toString(),
                          juce::String ("gold"));
            expectEquals ((int) bridge::hostVar (other).getProperty ("frameRate", 0), 30);
            UiSettings::get().setThemeId ("signal");
            UiSettings::get().setFrameRate (60);
        }

        beginTest ("hostVar carries live independently of SAFE mode word");
        {
            UiSettings::get().setLiveMode (true);
            NeuroKoreAudioProcessor proc;
            const auto host = bridge::hostVar (proc);
            expect ((bool) host.getProperty ("live", false));
            expect (host.hasProperty ("live"));
            UiSettings::get().setLiveMode (false);
            const auto off = bridge::hostVar (proc);
            expect (! (bool) off.getProperty ("live", true));
        }

        beginTest ("LIVE on one processor applies to another without an editor");
        {
            UiSettings::get().setLiveMode (false);
            NeuroKoreAudioProcessor a;
            NeuroKoreAudioProcessor b;
            a.prepareToPlay (48000.0, 512);
            b.prepareToPlay (48000.0, 512);
            expect (! a.isLiveMode());
            expect (! b.isLiveMode());
            a.setLiveMode (true);
            expect (a.isLiveMode());
            expect (b.isLiveMode());
            UiSettings::get().setLiveMode (false);
            expect (! a.isLiveMode());
            expect (! b.isLiveMode());
        }

        beginTest ("frame rate is 30 or 60, never display-uncapped");
        {
            expectEquals (UiSettings::clampFrameRate (0), 60);
            expectEquals (UiSettings::clampFrameRate (30), 30);
            expectEquals (UiSettings::clampFrameRate (60), 60);
            expectEquals (UiSettings::clampFrameRate (120), 60);
            UiSettings::get().setFrameRate (0);
            expectEquals (UiSettings::get().frameRate(), 60);
        }

        beginTest ("footer lat is the same number the host uses for PDC");
        {
            NeuroKoreAudioProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            const auto host = bridge::hostVar (proc);
            const int lat = (int) host.getProperty ("lat", -1);
            expectEquals (lat, proc.getLatencySamples());
            expect (lat >= proc.getOversamplingLatencySamples());
        }

        beginTest ("hostVar reports sidechainOn from the SC bus");
        {
            NeuroKoreAudioProcessor proc;
            const auto host = bridge::hostVar (proc);
            expect (host.hasProperty ("sidechainOn"));
            const bool on = (bool) host.getProperty ("sidechainOn", false);
            if (auto* bus = proc.getBus (true, 1))
                expect (on == bus->isEnabled());
            else
                expect (! on);
        }

        beginTest ("save lists and reloads a user preset");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("stage1: y = tanh(x * a)\n", err), err);

            const auto name = juce::String ("NkContractSave_") + juce::String (juce::Time::getMillisecondCounter());
            bridge::PresetCmd save;
            save.action = "save";
            save.name = name;
            save.author = "Kay";
            save.category = "Bass";
            expect (bridge::applyPresetCmd (proc, save, err), err);
            expectEquals (proc.getCurrentPresetName(), name);

            const auto state = bridge::presetStateVar (proc);
            const auto list = state.getProperty ("list", juce::var());
            bool found = false;
            for (int i = 0; i < list.size(); ++i)
            {
                if (list[i].getProperty ("name", "").toString() != name)
                    continue;
                found = true;
                expectEquals ((int) list[i].getProperty ("factory", true), 0);
                expectEquals (list[i].getProperty ("author", "").toString(), juce::String ("Kay"));
                expectEquals (list[i].getProperty ("category", "").toString(), juce::String ("Bass"));
            }
            expect (found, "user preset must appear in presetState list");

            NeuroKoreAudioProcessor loaded;
            bridge::PresetCmd load;
            load.action = "load";
            load.name = name;
            expect (bridge::applyPresetCmd (loaded, load, err), err);
            expect (loaded.getScript().contains ("tanh"), loaded.getScript());
            expectEquals (loaded.getCurrentPresetName(), name);

            auto file = PresetLibrary::userPresetRoot()
                            .getChildFile (name + juce::String (Config::kPresetFileExtension));
            expect (file.existsAsFile(), file.getFullPathName());
            file.deleteFile();
        }

        beginTest ("irVar lists compiled IR slots even when empty");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("ir1: mix = 0.3; gain = 0\n", err), err);
            const auto ir = bridge::irVar (proc);
            const auto slots = ir.getProperty ("slots", juce::var());
            expect (slots.isArray(), "ir payload has slots");
            expect (slots.size() >= 1, "ir1 must appear after compile");
            expectEquals (slots[0].getProperty ("slot", "").toString(), juce::String ("ir1"));
            expectEquals ((int) slots[0].getProperty ("loaded", 1), 0);
        }

        beginTest ("save refuses to overwrite a factory preset name");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                lib.loadFromEmbedded();
            expect (lib.getEntries().size() > 0);
            const auto factoryName = lib.getEntries().front().name;
            bridge::PresetCmd save;
            save.action = "save";
            save.name = factoryName;
            expect (! bridge::applyPresetCmd (proc, save, err));
            expect (err.containsIgnoreCase ("factory"));
        }

        beginTest ("applyKnobMeta writes note range into the script");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("param a = Time [20, 2000]\ndelay1: time = a\n", err), err);
            bridge::KnobMetaCmd m;
            m.id = "a";
            m.hasMin = true;
            m.hasMax = true;
            m.hasNote = true;
            m.min = 1.f;
            m.max = 0.0625f;
            m.isNote = true;
            expect (bridge::applyKnobMeta (proc, m, err), err);
            expect (proc.getScript().contains ("[1/1, 1/16]"), proc.getScript());
        }
    }
};
