#ifndef EDITORUXTEST_H
#define EDITORUXTEST_H

#include <JuceHeader.h>
#include <BinaryData.h>
#include <array>
#include <cmath>
#include "../src/ui/HelpContentComponent.h"
#include "../src/ui/FunctionsContentComponent.h"
#include "../src/ui/DslAutocomplete.h"
#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"
#include "../src/ui/FormulaDisplayComponent.h"
#include "../src/ui/PluginLookAndFeel.h"
#include "../src/ui/custom/BrandLockup.h"
#include "../src/ui/LoudnessMeterComponent.h"
#include "../src/ui/ScopeAnalytics.h"
#include "../src/ui/ScopeDeck.h"
#include "../src/utils/PresetSearch.h"
#include "../src/utils/PresetRatings.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "../src/ui/PresetTableComponent.h"
#include "../src/ui/PresetContentComponent.h"
#include "../src/ui/ModalOverlay.h"
#include "../src/ui/DslTerminalEditor.h"
#include "../src/ui/IrSlotUi.h"
#include "../src/ui/LicenseInfoComponent.h"
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
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
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
            expect (table.countInScope ({}) >= 189);
            expect (table.countInScope ("Club") >= 8);
            expect (table.getFilteredCount() == table.countInScope ({}));
        }

        beginTest ("preset explorer uses a folder sidebar");
        {
            expect (PresetContentComponent::kExplorerSidebarWidth >= 200);
            expect (PresetContentComponent::kCategoryRowHeight >= 28);
            expect (PresetContentComponent::kCategoryNameFontPt >= 16.f);
        }

        beginTest ("functions catalog folders split core from drive");
        {
            expect (FunctionsContentComponent::kExplorerSidebarWidth >= 180);
            expectEquals (FunctionsContentComponent::categoryForName ("sin"), juce::String ("Core"));
            expectEquals (FunctionsContentComponent::categoryForName ("lerp"), juce::String ("Core"));
            expectEquals (FunctionsContentComponent::categoryForName ("tube"), juce::String ("Drive"));
            expectEquals (FunctionsContentComponent::categoryForName ("diode"), juce::String ("Drive"));
            expectEquals (FunctionsContentComponent::categoryForName ("softclip"), juce::String ("Drive"));
            expectEquals (FunctionsContentComponent::categoryForName ("bitcrush"), juce::String ("Crush"));
            expectEquals (FunctionsContentComponent::categoryForName ("ott"), juce::String ("Blocks"));
            expectEquals (FunctionsContentComponent::categoryForName ("widen"), juce::String ("Blocks"));
            expectEquals (FunctionsContentComponent::categoryForName ("vocoder"), juce::String ("Blocks"));
        }

        beginTest ("formula comments use rust, not editor green");
        {
            const auto c = NeuroCoreLookAndFeel::comment();
            expect (c.getRed() > c.getGreen());
            expect (c.getRed() > 0x90);
            expect (c.getGreen() < 0x90);
        }

        beginTest ("modal overlay panel and scrim grow with the host");
        {
            const auto small = ModalOverlay::panelSizeFor (1280, 860, 0, 0);
            const auto large = ModalOverlay::panelSizeFor (1800, 1200, 0, 0);
            expect (small.getWidth() > 1200);
            expect (large.getWidth() > small.getWidth());
            expect (large.getHeight() > small.getHeight());
            expectEquals (large.getWidth(), 1800 - 24);
            expectEquals (large.getHeight(), 1200 - 24);
            const auto capped = ModalOverlay::panelSizeFor (1800, 1200, 520, 400);
            expectEquals (capped.getWidth(), 520);
            expectEquals (capped.getHeight(), 400);
        }

        beginTest ("OS banner is Neuroklast OS");
        {
            const juce::String banner (Config::kOsBanner);
            expect (banner.containsIgnoreCase ("neurokore"));
            expect (banner.containsIgnoreCase ("neuroklast"));
            expect (! banner.containsIgnoreCase ("netrunner"));
            expectEquals (juce::String (Config::kProductName), juce::String ("NEUROKORE"));
            expectEquals (juce::String (Config::kBrandByline), juce::String ("by Neuroklast"));
            expectEquals (juce::String (Config::kAppDataFolder), juce::String ("NeuroKore"));
            expectEquals (juce::String (PLUGIN_ID), juce::String ("nrko01"));
            expectEquals (juce::String (PLUGIN_VERSION), juce::String ("0.9.0"));
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

        beginTest ("preset chip fills the compact toolbar");
        {
            expect (Config::kPresetChipFontPt >= 14.f);
            expect (Config::kPresetChipFontPt <= 16.f);
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

        beginTest ("toolbar is half the previous 0.09 weight and stays under the HUD");
        {
            expect (Config::kHudHeaderHeight == 22);
            expectEquals (Config::kToolbarRowWeight, 0.045f);
            expect (Config::kToolbarRowMaxHeight * 2 <= 80);
            expect (Config::kToolbarRowMaxHeight <= 38);
            expect (Config::kToolbarRowMinHeight >= 32);
            expect (BrandLockup::kMaxLogoHeight <= (float) Config::kToolbarRowMaxHeight);
            expectEquals (BrandLockup::kMaxLogoHeight, 26.f);
        }

        beginTest ("NK logo asset is edge-cropped landscape");
        {
            auto img = NeuroCoreLookAndFeel::cropOpaqueContent (
                juce::ImageCache::getFromMemory (BinaryData::nk_logo_png,
                                                 BinaryData::nk_logo_pngSize));
            expect (img.isValid());
            expect (img.getWidth() > 0 && img.getHeight() > 0);
            expect (img.getWidth() * 10 > img.getHeight() * 14);
        }

        beginTest ("stereo field stats: mono, invert, silent");
        {
            const float L[8] = { 0.50f, -0.50f, 0.25f, -0.25f, 0.50f, -0.50f, 0.25f, -0.25f };
            const auto mono = ScopeAnalytics::analyse (L, L, 8);
            expect (mono.correlation > 0.99f);
            expect (mono.width < 0.02f);
            expect (mono.peakL > 0.49f);

            float inv[8];
            for (int i = 0; i < 8; ++i)
                inv[i] = -L[i];
            const auto side = ScopeAnalytics::analyse (L, inv, 8);
            expect (side.correlation < -0.99f);
            expect (side.width > 0.98f);

            const float z[4] = {};
            const auto silent = ScopeAnalytics::analyse (z, z, 4);
            expectEquals (silent.correlation, 0.f);
            expectEquals (silent.peakL, 0.f);
            expectEquals (silent.peakDbL, -100.f);

            const auto g = ScopeAnalytics::gonioPoint (0.5f, 0.5f);
            expect (std::abs (g.x) < 1.0e-5f);
            expect (g.y < 0.f);

            // L or R input copies that side onto both channels → vertical line.
            const auto line = ScopeAnalytics::analyse (L, L, 8);
            const auto gR = ScopeAnalytics::gonioPoint (0.4f, 0.4f);
            expect (line.correlation > 0.99f);
            expect (std::abs (gR.x) < 1.0e-5f);
        }

        beginTest ("scope extras sit in the wave row and can fold");
        {
            expect (Config::kScopeFoldWidth >= 14);
            expect (Config::kScopeLoudnessWidth >= 40);
            expect (Config::kScopeFieldMinWidth >= 72);
            const int extras = Config::kScopeFoldWidth
                             + Config::kScopeLoudnessWidth
                             + Config::kScopeFieldMinWidth;
            expect (extras < 200);
            expect (extras > 100);

            NeuroCoreAudioProcessor proc;
            ScopeDeck deck (proc, WaveformDisplayComponent::Type::Input);
            deck.setSize (600, 120);
            expect (deck.extrasOpen());
            expect (deck.waveform().getWidth() < 600 - Config::kScopeFoldWidth);
            deck.setExtrasOpen (false);
            expect (! deck.extrasOpen());
            expect (deck.waveform().getWidth() >= 600 - Config::kScopeFoldWidth - 2);
        }

        beginTest ("licensed License click shows the holder email");
        {
            expectEquals (LicenseInfoUi::holderLine ("  ada@neuroklast.net "),
                          juce::String ("ada@neuroklast.net"));
            expectEquals (LicenseInfoUi::holderLine ({}),
                          juce::String ("Unknown holder"));
            expectEquals (LicenseInfoUi::issuedLine ("2026-08-13"),
                          juce::String ("Issued  2026-08-13"));
            expect (LicenseInfoUi::issuedLine ({}).isEmpty());
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
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
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
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            int withHeader = 0;
            bool acidCommented = false;
            for (const auto& e : lib.getEntries())
            {
                if (e.script.contains ("# " + e.name)
                    && e.script.containsIgnoreCase ("How it sounds"))
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
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
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
            const auto f = juce::File (NEUROKORE_RESOURCES_DIR).getChildFile ("UserManual_en.txt");
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
            expect (body.contains ("gate"));
            expect (body.contains ("limit"));
            expect (body.contains ("ir1"));
            expect (body.contains ("licensed to"));
            expect (body.contains (".zip") || body.contains ("zip pack"));
            expect (body.contains ("ctrl+space") || body.contains ("ctrl + space"));
            expect (body.contains ("locked while bypass") || body.contains ("locks the mix"));
        }

        beginTest ("Help body font is large enough to read");
        {
            expect (Config::kHelpBodyFontPt >= 20.f);
            expect (Config::kHelpListFontPt >= 16.f);
            HelpContentComponent help ("# Title\n\nintro\n\n## 1. Quickstart\nDo this.\n");
            help.showChapter (1);
            expect (help.getBodyFontHeight() >= 19.5f);
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
