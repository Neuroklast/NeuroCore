#ifndef EDITORUXTEST_H
#define EDITORUXTEST_H

#include <JuceHeader.h>
#include <array>
#include "../src/ui/HelpContentComponent.h"
#include "../src/ui/DslAutocomplete.h"
#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/ui/FormulaDisplayComponent.h"
#include "../src/ui/PluginLookAndFeel.h"
#include "../src/ui/custom/BrandLockup.h"
#include "../src/ui/LoudnessMeterComponent.h"
#include "../src/utils/PresetSearch.h"
#include "../src/utils/PresetRatings.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "../src/ui/PresetTableComponent.h"
#include "../src/ui/DslTerminalEditor.h"
#include "../src/ui/IrSlotUi.h"
#include "../src/core/PluginProcessor.h"

class EditorUxTest : public juce::UnitTest
{
public:
    EditorUxTest() : juce::UnitTest ("EditorUxTest", "UI") {}

    void runTest() override
    {
        beginTest ("Preset names sort naturally by column");
        {
            juce::StringArray names { "Haas Width", "Amp Crunch", "Score Hall" };
            names.sortNatural();
            expectEquals (names[0], juce::String ("Amp Crunch"));
            expectEquals (names[2], juce::String ("Score Hall"));
        }

        beginTest ("preset table defaults to name sort and shows a tags column");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROCORE_RESOURCES_DIR)));
            NeuroCoreAudioProcessor proc;
            PresetTableComponent table (proc);
            auto& header = table.getTable().getHeader();
            expectEquals (header.getSortColumnId(), 1);
            expect (header.isSortedForwards());
            expect (header.getNumColumns (true) >= 6);
            expect (header.getColumnName (6).containsIgnoreCase ("tag"));
            expect (table.getNumRows() > 0);
            if (table.getNumRows() >= 2)
            {
                const auto a = table.getNameForRow (0);
                const auto b = table.getNameForRow (1);
                expect (a.compareNatural (b) <= 0, a + " should sort before " + b);
            }
            bool sawTags = false;
            for (int r = 0; r < juce::jmin (40, table.getNumRows()); ++r)
                if (table.getTagsForRow (r).size() > 0)
                    sawTags = true;
            expect (sawTags, "factory rows should expose tags in the table");
        }

        beginTest ("OS banner is Neuroklast OS");
        {
            const juce::String banner (Config::kOsBanner);
            expect (banner.containsIgnoreCase ("neuroklast"));
            expect (! banner.containsIgnoreCase ("netrunner"));
        }

        beginTest ("preset ratings clamp to 1-5 and clear at 0");
        {
            auto& r = PresetRatings::getInstance();
            const juce::String key ("__unit_test_rating__");
            r.set (key, 9);
            expectEquals (r.get (key), 5);
            r.set (key, 3);
            expectEquals (r.get (key), 3);
            r.set (key, 0);
            expectEquals (r.get (key), 0);
        }

        beginTest ("editor column is wider than the knob column");
        {
            expect (Config::kBodyEditorWeight > Config::kBodyKnobsWeight);
            expect (Config::kBodyKnobsWeight < 2.4f);
        }

        beginTest ("meter pixel bands stay fine at idle and chunk at the top");
        {
            expectEquals (LoudnessMeterComponent::bandHeightPx (0.f, 0.f), 2);
            expectEquals (LoudnessMeterComponent::bandHeightPx (0.f, 1.f), 2);
            expect (LoudnessMeterComponent::glitchAmount (0.2f) < 0.02f);
            expect (LoudnessMeterComponent::glitchAmount (0.35f) < 0.02f);
            expect (LoudnessMeterComponent::glitchAmount (1.f)
                    > LoudnessMeterComponent::glitchAmount (0.7f));
            expect (LoudnessMeterComponent::bandHeightPx (0.25f, 1.f)
                    < LoudnessMeterComponent::bandHeightPx (0.85f, 1.f));
            expect (LoudnessMeterComponent::bandHeightPx (0.5f, 1.f)
                    < LoudnessMeterComponent::bandHeightPx (1.f, 1.f));
            expect (LoudnessMeterComponent::bandHeightPx (1.f, 1.f) >= 6);
        }

        beginTest ("preset chip uses a large brand font");
        {
            expect (Config::kPresetChipFontPt >= 26.f);
        }

        beginTest ("status numeric fields keep a fixed character width");
        {
            const auto cpuLo = juce::String (3).paddedLeft (' ', 3);
            const auto cpuHi = juce::String (100).paddedLeft (' ', 3);
            expectEquals (cpuLo.length(), cpuHi.length());
            expectEquals (cpuLo.length(), 3);
            const auto mixLo = juce::String (5).paddedLeft (' ', 3);
            const auto mixHi = juce::String (100).paddedLeft (' ', 3);
            expectEquals (mixLo.length(), mixHi.length());
            const auto modeA = juce::String ("LIVE").paddedRight (' ', 6);
            const auto modeB = juce::String ("BYPASS").paddedRight (' ', 6);
            expectEquals (modeA.length(), modeB.length());
        }

        beginTest ("NK lockup stays smaller than the HUD strip");
        {
            expect (Config::kHudHeaderHeight == 22);
            expect (BrandLockup::kMaxLogoHeight <= 20.f);
            expect (BrandLockup::kMaxLogoHeight < (float) Config::kHudHeaderHeight);
        }

        beginTest ("NK lockup opens the Neuroklast site");
        {
            expect (juce::String (BrandLockup::kWebsiteUrl)
                        == "https://neuroklast.net");
            expect (juce::URL (BrandLockup::kWebsiteUrl).isWellFormed());
        }

        beginTest ("Preset search matches tags and DSL tokens");
        {
            const juce::String script =
                "ms1: mode = encode\n"
                "stage1: channel = mid; y = x\n"
                "delay1: time = 220; channel = side\n"
                "ms2: mode = decode\n";
            const auto tags = PresetSearch::inferTags (script, "Side Delay", "Delay",
                                                       "MS delay on the side");
            expect (tags.contains ("delay", true));
            expect (tags.contains ("mid-side", true) || tags.contains ("ms", true));
            const auto hay = PresetSearch::buildHaystack ("Side Delay", "Delay", "width",
                                                          "NEUROKLAST", tags, script);
            expect (PresetSearch::matches (hay, "mid side"));
            expect (PresetSearch::matches (hay, "delay"));
            expect (PresetSearch::matches (hay, "MS"));
            expect (! PresetSearch::matches (hay, "bitcrush vocal"));
        }

        beginTest ("factory library has hardware dynamics and rooms");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROCORE_RESOURCES_DIR)));
            juce::StringArray need {
                "Multiband Glue", "Envelope Shaper", "1176 FET", "1176 All In",
                "LA-2A Opto", "SSL Bus Comp", "Fairchild Mu", "dbx 160 VCA",
                "CL-1B Vocal", "Neve Diode Bus", "Space Echo RE-201",
                "Memory Man BBD", "Echoplex EP-3", "TC 2290 Grid",
                "EMT 140 Plate", "Lexicon 480 Hall", "AMS RMX Nonlin", "Spring Tank"
            };
            juce::StringArray have;
            for (const auto& e : lib.getEntries())
                have.add (e.name);
            for (const auto& n : need)
                expect (have.contains (n), "missing factory preset: " + n);
            bool tubeScreamerHasPeak = false;
            bool bassKeepsPeak = false;
            for (const auto& e : lib.getEntries())
            {
                if (e.name == "Tube Screamer")
                    tubeScreamerHasPeak = e.script.containsIgnoreCase ("eq")
                                       && e.script.containsIgnoreCase ("peak")
                                       && ! e.script.contains ("type = bandpass");
                if (e.name == "Bass Architect")
                    bassKeepsPeak = e.script.containsIgnoreCase ("eq")
                                 && e.script.containsIgnoreCase ("peak");
            }
            expect (tubeScreamerHasPeak, "Tube Screamer must use a mid peak, not a bandpass");
            expect (bassKeepsPeak, "Bass Architect mid must be a peak so the sub stays");
        }

        beginTest ("factory scripts carry operator comments");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROCORE_RESOURCES_DIR)));
            int withHeader = 0;
            bool acidCommented = false;
            for (const auto& e : lib.getEntries())
            {
                if (e.script.contains ("# " + e.name))
                    ++withHeader;
                if (e.name == "Acid Line")
                    acidCommented = e.script.contains ("# Acid Line")
                                 && (e.script.containsIgnoreCase ("# a ")
                                     || e.script.contains ("# diode")
                                     || e.script.contains ("# lowpass"));
            }
            expectEquals (withHeader, (int) lib.getEntries().size());
            expect (acidCommented);
        }

        beginTest ("Factory library is searchable by tape and mid side");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROCORE_RESOURCES_DIR)));
            bool foundMs = false, foundTape = false, sideDelayOk = false;
            for (const auto& e : lib.getEntries())
            {
                const auto hay = PresetSearch::buildHaystack (e.name, e.category, e.description,
                                                              "NEUROKLAST", e.tags, e.script);
                if (e.name == "Side Delay")
                    sideDelayOk = PresetSearch::matches (hay, "mid side")
                               && PresetSearch::matches (hay, "delay");
                if (PresetSearch::matches (hay, "tape"))
                    foundTape = true;
                if (PresetSearch::matches (hay, "mid side"))
                    foundMs = true;
                expect (e.tags.size() > 0, e.name + " should carry tags");
            }
            expect (sideDelayOk, "Side Delay must match mid side + delay");
            expect (foundMs);
            expect (foundTape);
        }

        beginTest ("Help parser splits ## chapters");
        {
            const juce::String md =
                "# Title\n\nintro\n\n## 1. Quickstart\nDo this.\n\n## 2. Knobs\nTurn a.\n";
            const auto ch = parseHelpChapters (md);
            expectEquals ((int) ch.size(), 3);
            expect (ch[0].title.contains ("Overview"));
            expect (ch[1].title.contains ("Quickstart"));
            expect (ch[2].title.contains ("Knobs"));
            expect (ch[1].body.contains ("Do this"));
        }

        beginTest ("Help markdown strips stars headings and rules");
        {
            const juce::String md =
                "### Factory\n\n"
                "- Author: **NEUROKLAST**.\n\n"
                "---\n\n"
                "Shipped in `factory_presets.json`.\n";
            const auto plain = stripMarkdownToPlain (md);
            expect (! plain.contains ("**"));
            expect (! plain.contains ("###"));
            expect (! plain.contains ("---"));
            expect (! plain.contains ("`"));
            expect (plain.contains ("NEUROKLAST"));
            expect (plain.contains ("factory_presets.json"));
            expect (plain.contains ("Factory"));
        }

        beginTest ("Help tables stack as definitions not em-dash walls");
        {
            const juce::String md =
                "| Area | Purpose |\n"
                "|------|---------|\n"
                "| **Presets** | Open the library |\n"
                "| Mix | Dry/wet blend |\n";
            const auto plain = stripMarkdownToPlain (md);
            expect (! plain.contains ("—"));
            expect (! plain.contains ("Area  —  Purpose"));
            expect (plain.contains ("Presets"));
            expect (plain.contains ("Open the library"));
            expect (plain.contains ("Mix"));
            expect (plain.contains ("Dry/wet blend"));
        }

        beginTest ("In-plugin Help stays user-facing");
        {
            const auto f = juce::File (NEUROCORE_RESOURCES_DIR).getChildFile ("UserManual_en.txt");
            expect (f.existsAsFile(), "UserManual_en.txt must exist");
            const auto body = f.loadFileAsString().toLowerCase();
            expect (body.isNotEmpty());
            const char* banned[] = {
                "juce", "cmake", "apvts", "adaa", "factory_presets.json",
                "dsl_reference", "jetbrains", "apex", "under lock",
                "send dag", "cse", "compile the", "repository"
            };
            for (auto* word : banned)
                expect (! body.contains (word),
                        juce::String ("Help still mentions engineering term: ") + word);
            expect (body.contains ("presets"));
            expect (body.contains ("mix"));
            expect (body.contains ("tutorial"));
            expect (body.contains ("eq"));
            expect (body.contains ("octaver"));
            expect (body.contains ("vocoder"));
            expect (body.contains ("ctrl+space") || body.contains ("ctrl + space"));
            expect (body.contains ("locked while bypass") || body.contains ("locks the mix"));
        }

        beginTest ("Help body font is large enough to read");
        {
            expect (Config::kHelpBodyFontPt >= 16.f);
            HelpContentComponent help ("# Title\n\nintro\n\n## 1. Quickstart\nDo this.\n");
            help.showChapter (1);
            expect (help.getBodyFontHeight() >= 15.5f);
        }

        beginTest ("formula editor keeps autocomplete closed until Ctrl+Space");
        {
            NeuroCoreAudioProcessor proc;
            DslTerminalEditor ed (proc);
            ed.setText ("filter1: type = ");
            ed.setCaretPosition (ed.getText().length());
            expect (! ed.isSuggestionPopupVisible());
        }

        beginTest ("IR slot ids are detected per DSL line");
        {
            expectEquals (IrSlotUi::slotFromLine ("ir1: mix = 1"), juce::String ("ir1"));
            expectEquals (IrSlotUi::slotFromLine ("  ir2: mix = 0.5"), juce::String ("ir2"));
            expectEquals (IrSlotUi::slotFromLine ("convolve: mix = 1"), juce::String ("convolve"));
            expect (IrSlotUi::slotFromLine ("iron: y = x").isEmpty());
            expect (IrSlotUi::slotFromLine ("stage1: y = x").isEmpty());
            expect (IrSlotUi::slotFromLine ("# ir1: mix = 1").isEmpty());
            expect (IrSlotUi::slotFromLine ("// ir2: mix = 1").isEmpty());
        }

        beginTest ("formula display adds one full-width IR button per slot");
        {
            FormulaDisplayComponent d;
            d.setSize (400, 300);
            d.setFormula ("ir1: mix = 1\nstage1: y = x\nir2: mix = 0.5\n");
            expectEquals (d.getIrButtonSlots().size(), 2);
            expectEquals (d.getIrButtonSlots()[0], juce::String ("ir1"));
            expectEquals (d.getIrButtonSlots()[1], juce::String ("ir2"));
            d.setFormula ("stage1: y = x\n");
            expectEquals (d.getIrButtonSlots().size(), 0);
        }

        beginTest ("code editor adds one IR button per irN line");
        {
            NeuroCoreAudioProcessor proc;
            DslTerminalEditor ed (proc);
            ed.setSize (400, 300);
            ed.setText ("ir1: mix = 1\nstage1: y = x\nir2: mix = 0.4\n");
            ed.refreshIrButtons();
            expectEquals (ed.getIrButtonSlots().size(), 2);
            expectEquals (ed.getIrButtonSlots()[0], juce::String ("ir1"));
            expectEquals (ed.getIrButtonSlots()[1], juce::String ("ir2"));
        }

        beginTest ("Help body uses JetBrains Mono not Apex");
        {
            HelpContentComponent help ("# Title\n\nintro\n\n## 1. Quickstart\nDo this.\n");
            help.showChapter (1);
            const auto face = help.getBodyTypefaceName();
            expect (face.containsIgnoreCase ("Mono") || face.containsIgnoreCase ("JetBrains"),
                    "Help body must use embedded mono, got: " + face);
            expect (! face.containsIgnoreCase ("Apex"));
        }

        beginTest ("NK logo crop drops black padding");
        {
            juce::Image src (juce::Image::ARGB, 40, 40, true);
            src.clear (src.getBounds(), juce::Colours::black);
            {
                juce::Graphics g (src);
                g.setColour (juce::Colour (0xffff1a1a));
                g.fillRect (10, 12, 16, 10);
            }
            const auto cropped = NeuroCoreLookAndFeel::cropOpaqueContent (src);
            expect (cropped.getWidth() < 40);
            expect (cropped.getHeight() < 40);
            expect (cropped.getWidth() >= 16);
            expect (cropped.getHeight() >= 10);
        }

        beginTest ("Help click shows only that chapter");
        {
            const juce::String md =
                "# Title\n\nintro\n\n## 1. Quickstart\nDo this.\n\n## 2. Knobs\nTurn a.\n";
            HelpContentComponent help (md);
            expect (help.getVisibleChapterCount() >= 2);
            help.showChapter (1);
            const auto shown = help.getDisplayedText();
            expect (shown.contains ("Quickstart") || shown.contains ("Do this")
                    || shown.contains ("Knobs"));
            help.showChapter (2);
            const auto knobs = help.getDisplayedText();
            expect (knobs.contains ("Knobs"));
            expect (knobs.contains ("Turn a"));
            expect (! knobs.contains ("Do this"));
        }

        std::array<juce::String, Config::kNumUserParams> names {};
        names[0] = "Drive";

        beginTest ("Autocomplete filter type is filter-only");
        {
            const juce::String s = "filter1: type = ";
            const auto items = DslAutocomplete::complete (s, s.length(), names, false);
            bool hasLp = false, hasSine = false, hasSin = false;
            for (const auto& it : items)
            {
                if (it.label == "lowpass") hasLp = true;
                if (it.label == "sine") hasSine = true;
                if (it.label == "sin") hasSin = true;
            }
            expect (hasLp);
            expect (! hasSine);
            expect (! hasSin);
        }

        beginTest ("Autocomplete does not suggest in/main as blocks");
        {
            const juce::String s = "st";
            const auto items = DslAutocomplete::complete (s, s.length(), names, false);
            for (const auto& it : items)
            {
                expect (it.label != "in");
                expect (it.label != "main");
            }
        }

        beginTest ("Input channel switch maps L / Both / R");
        {
            bool l = true, r = true;
            expect (EffectParameters::modeFromFlags (true, true)
                    == EffectParameters::InputChannelMode::Both);
            expect (EffectParameters::modeFromFlags (true, false)
                    == EffectParameters::InputChannelMode::Left);
            expect (EffectParameters::modeFromFlags (false, true)
                    == EffectParameters::InputChannelMode::Right);
            expect (EffectParameters::modeFromFlags (false, false)
                    == EffectParameters::InputChannelMode::Both);
            EffectParameters::flagsFromMode (EffectParameters::InputChannelMode::Left, l, r);
            expect (l && ! r);
            EffectParameters::flagsFromMode (EffectParameters::InputChannelMode::Right, l, r);
            expect (! l && r);
            EffectParameters::flagsFromMode (EffectParameters::InputChannelMode::Both, l, r);
            expect (l && r);
        }

        beginTest ("Formula live view scrolls when taller than the panel");
        {
            expectEquals (Config::kFormulaLineHeight, 1.1f);
            FormulaDisplayComponent view;
            view.setBounds (0, 0, 420, 140);
            juce::String script;
            for (int i = 0; i < 40; ++i)
                script << "stage" << i << ": y = x\n";
            view.setFormula (script);
            expect (view.getContentHeight() > view.getHeight());
        }

        beginTest ("Autocomplete send only inside a bus");
        {
            const juce::String top = "se";
            const auto a = DslAutocomplete::complete (top, top.length(), names, false);
            bool sendTop = false;
            for (const auto& it : a)
                if (it.label == "send") sendTop = true;
            expect (! sendTop);

            const juce::String inside = "bus dirt:\nse";
            const auto b = DslAutocomplete::complete (inside, inside.length(), names, false);
            bool sendIn = false;
            for (const auto& it : b)
                if (it.label == "send") sendIn = true;
            expect (sendIn);
        }
    }
};

#endif
