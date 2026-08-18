#ifndef EDITORUXTEST_H
#define EDITORUXTEST_H

#include <JuceHeader.h>
#include <BinaryData.h>
#include <array>
#include <cmath>
#include "../src/ui/HelpContentComponent.h"
#include "../src/ui/FunctionsContentComponent.h"
#include "../src/ui/DslAutocomplete.h"
#include "../src/ui/DslTokeniser.h"
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
#include "../src/utils/FormulaHelper.h"
#include "../src/ui/SettingsContentComponent.h"
#include "../src/utils/UiSettings.h"
#include "../src/core/PluginProcessor.h"
#include "../src/ui/GraphCanvasComponent.h"
#include "../src/ui/custom/ParameterComponent.h"
#include "../src/dsl/GraphModel.h"
#include "../src/ui/PluginEditor.h"
#include "../src/ui/StagesContentComponent.h"

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
            NeuroKoreAudioProcessor proc;
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

        beginTest ("formula comments stay red/white/black, not rust");
        {
            expect (NeuroKoreLookAndFeel::comment() == NeuroKoreLookAndFeel::inkMuted());
        }

        beginTest ("look and feel splits chrome accent from body ink");
        {
            const auto canvas = NeuroKoreLookAndFeel::canvas();
            const auto ink = NeuroKoreLookAndFeel::ink();
            const auto muted = NeuroKoreLookAndFeel::inkMuted();
            const auto accent = NeuroKoreLookAndFeel::accent();
            expect (canvas.getBrightness() < 0.08f);
            expect (ink.getBrightness() > 0.85f);
            expect (muted.getBrightness() > juce::Colour (0xff8a8a8a).getBrightness());
            expect (muted.getPerceivedBrightness() > 0.7f);
            expect (NeuroKoreLookAndFeel::mutedText() == muted);
            expect (NeuroKoreLookAndFeel::brightText() == ink);
            expect (NeuroKoreLookAndFeel::warningMark() == accent);
            expect (NeuroKoreLookAndFeel::error() != accent);
            expect (FormulaDisplayComponent::knobColour (3) == NeuroKoreLookAndFeel::accent());
            expect (FormulaDisplayComponent::knobColour (0) == FormulaDisplayComponent::knobColour (5));
        }

        beginTest ("UI design raster is larger than the old unscaled minimum");
        {
            expectEquals (Config::kUiDesignWidth, Config::kWindowWidth);
            expectEquals (Config::kUiDesignHeight, Config::kWindowHeight);
            expect (Config::kUiMinWindowWidth > 960);
            expect (Config::kUiMinWindowHeight > 640);
            expect (Config::kUiScalePercentMax > Config::kUiScalePercentMin);
        }

        beginTest ("modal overlay panel and scrim grow with the host");
        {
            const auto small = ModalOverlay::panelSizeFor (1280, 860, 0, 0);
            const auto large = ModalOverlay::panelSizeFor (1800, 1200, 0, 0);
            expect (small.getWidth() > 1200);
            expect (large.getWidth() > small.getWidth());
            expect (large.getHeight() > small.getHeight());
            expectEquals (large.getWidth(), 1800 - 16);
            expectEquals (large.getHeight(), 1200 - 16);
            const auto capped = ModalOverlay::panelSizeFor (1800, 1200, 520, 400);
            expectEquals (capped.getWidth(), 520);
            expectEquals (capped.getHeight(), 400);
            expect (Config::kOverlayTopChromeDesign
                    > Config::kHudHeaderHeight);
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
            expectEquals (juce::String (PLUGIN_VERSION), juce::String ("0.4.8-alpha"));
            expectEquals (Config::cpuDisplayPercent (0.f), 0);
            expectEquals (Config::cpuDisplayPercent (0.42f), 42);
            expectEquals (Config::cpuDisplayPercent (1.73f), 100);
            expectEquals (Config::cpuDisplayPercent (12.f), 100);
        }

        beginTest ("fold hit matches the painted chevron");
        {
            const auto chev = GraphCanvasComponent::foldChevronRect (176, 64, false, 1.f);
            const auto hit = GraphCanvasComponent::foldHitRect (176, 64, false, 1.f);
            expect (hit.contains (chev.getCentre()));
            expect (GraphCanvasComponent::chipHeight (1, 3, 0, 0, false)
                    > GraphCanvasComponent::chipHeight (1, 1, 0, 0, false));
        }

        beginTest ("host-scale fit snaps so the 16 px board grid is integer on screen");
        {
            expectEquals (Config::kUiBoardGrid, 16);
            expectWithinAbsoluteError (Config::snapUiFitToGrid (1.f), 1.f, 1.0e-6f);
            expectWithinAbsoluteError (Config::snapUiFitToGrid (1.25f), 1.25f, 1.0e-6f);
            expectWithinAbsoluteError (Config::snapUiFitToGrid (1.5f), 1.5f, 1.0e-6f);
            expectWithinAbsoluteError (Config::snapUiFitToGrid (1.173f), 18.f / 16.f, 1.0e-6f);
            expectWithinAbsoluteError (NeuroKoreAudioProcessorEditor::snapFitToGrid (1.173f),
                                       Config::snapUiFitToGrid (1.173f), 1.0e-6f);
            const float samples[] = { 0.5f, 0.8f, 1.f, 1.07f, 1.173f, 1.25f, 1.333f, 1.5f };
            for (float raw : samples)
            {
                const float fit = Config::snapUiFitToGrid (raw);
                const float cells = fit * (float) Config::kUiBoardGrid;
                expect (std::abs (cells - std::round (cells)) < 1.0e-4f,
                        "fit=" + juce::String (fit, 5) + " cells=" + juce::String (cells, 5));
            }
        }

        beginTest ("host-scale fit never paints the canvas larger than the window");
        {
            const int pairs[][2] = { { 1270, 850 }, { 1900, 1000 }, { 1100, 739 }, { 1280, 860 } };
            for (const auto& wh : pairs)
            {
                const float raw = juce::jmin ((float) wh[0] / (float) Config::kUiDesignWidth,
                                              (float) wh[1] / (float) Config::kUiDesignHeight);
                const float fit = Config::snapUiFitToGrid (raw);
                expect ((float) Config::kUiDesignWidth * fit <= (float) wh[0] + 0.5f);
                expect ((float) Config::kUiDesignHeight * fit <= (float) wh[1] + 0.5f);
            }
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
            expect (Config::kPresetChipFontPt >= 12.f);
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

        beginTest ("toolbar is a thin rail so the editor owns the window");
        {
            expect (Config::kHudHeaderHeight <= 16);
            expect (Config::kToolbarRowMaxHeight <= 38);
            expect (Config::kToolbarRowMinHeight >= 28);
            expect (Config::kBodyEditorWeight > Config::kBodyKnobsWeight * 4.f);
            expect (Config::kParameterKnobSize <= 80);
            expect (Config::kScopeRowHeight >= 168);
            expect (Config::kBodyKnobsWeight >= 1.0f);
            expect (Config::kBodyKnobsWeight <= 1.4f);
            expect (BrandLockup::kMaxLogoHeight <= (float) Config::kToolbarRowMaxHeight + 2.f);
        }

        beginTest ("L/Both/R cells have a gap and fill the plate");
        {
            NeuroKoreAudioProcessor proc;
            InputChannelSwitch sw (proc.apvts);
            sw.setSize (140, 26);
            const auto a = sw.cellBounds (0);
            const auto b = sw.cellBounds (1);
            const auto c = sw.cellBounds (2);
            expect (b.getX() >= a.getRight() + InputChannelSwitch::kCellGap - 0.1f);
            expect (c.getX() >= b.getRight() + InputChannelSwitch::kCellGap - 0.1f);
            expect (c.getRight() <= sw.plateBounds().getRight() + 0.1f);
            expect (a.getWidth() > 30.f);
        }

        beginTest ("knob value sits below the rotary, not on the needle");
        {
            NeuroKoreAudioProcessor proc;
            ui::ParameterComponent pc (proc.apvts, "a", "Drive");
            pc.setSize (90, 130);
            expect (pc.getValueBounds().getY() >= pc.getSliderBounds().getBottom() - 1);
            expect (pc.getValueBounds().getHeight() >= 14);
            expect (! pc.isActivelyUsed());
        }

        beginTest ("factory script match names an untitled processor");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            const auto* sample = lib.findByName ("Shimmer Drive");
            expect (sample != nullptr);
            if (sample == nullptr)
                return;
            expect (lib.findMatchingScript (sample->script) == sample);
            NeuroKoreAudioProcessor proc;
            expect (proc.getCurrentPresetName().isEmpty());
            juce::String err;
            expect (proc.setFormula (sample->script, err, true));
            expect (proc.getCurrentPresetName().isEmpty());
            expect (proc.resolvePresetNameFromScript());
            expectEquals (proc.getCurrentPresetName(), sample->name);
        }

        beginTest ("NK logo asset is edge-cropped landscape");
        {
            auto img = NeuroKoreLookAndFeel::cropOpaqueContent (
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
            expect (Config::kScopeFieldMinWidth >= 88);
            const int extras = Config::kScopeFoldWidth
                             + Config::kScopeLoudnessWidth
                             + Config::kScopeFieldMinWidth;
            expect (extras < 200);
            expect (extras > 100);

            NeuroKoreAudioProcessor proc;
            ScopeDeck deck (proc, WaveformDisplayComponent::Type::Input);
            deck.setSize (600, 120);
            expect (deck.extrasOpen());
            expect (deck.waveform().getWidth() < 600 - Config::kScopeFoldWidth);
            deck.setExtrasOpen (false);
            expect (! deck.extrasOpen());
            expect (deck.waveform().getWidth() >= 600 - Config::kScopeFoldWidth - 2);
            deck.setExtrasOpen (true);
            expect (deck.extrasOpen());
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
                "Multiband Glue", "Envelope Shaper", "FET Peak Comp", "All Buttons Comp",
                "Optical Leveler Comp", "VCA Bus Glue", "Vari-Mu Bus",
                "Tube Opto Vocal", "Diode Bridge Bus", "Tape Echo Heads",
                "Analog Bucket Echo", "Tape Slap Echo", "Digital Grid Delay",
                "Foil Plate", "Large Hall", "Nonlinear Snap", "Spring Tank"
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
                if (e.name == "Mid Boost OD")
                    tubeScreamerHasPeak = e.script.containsIgnoreCase ("eq")
                                       && e.script.containsIgnoreCase ("peak")
                                       && ! e.script.contains ("type = bandpass");
                if (e.name == "Bass Architect")
                    bassKeepsPeak = e.script.containsIgnoreCase ("eq")
                                 && e.script.containsIgnoreCase ("peak");
            }
            expect (tubeScreamerHasPeak, "Mid Boost OD must use a mid peak, not a bandpass");
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
            NeuroKoreAudioProcessor proc;
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

        beginTest ("formula live view annotates mapped knob values");
        {
            FormulaDisplayComponent d;
            d.setFormula ("param a = Drive [0, 10]\nstage1: y = a * x\n");
            std::array<float, Config::kNumUserParams> knobs {};
            knobs[0] = 0.5f;
            d.setKnobValues (knobs);
            const auto text = d.getAnnotatedText();
            expect (text.contains ("=> 5"), "param line should show mapped live value, got: " + text);
            expect (text.contains ("a[5.000]"), "knob token should carry [value], got: " + text);
        }

        beginTest ("script workspace shows live formula values, not the text editor");
        {
            auto proc = std::make_unique<NeuroKoreAudioProcessor>();
            auto ed = std::make_unique<NeuroKoreAudioProcessorEditor> (*proc);
            expectEquals (ed->getWorkspaceMode(), (int) NeuroKoreAudioProcessorEditor::WorkspaceGraph);
            {
                const auto foot = ed->statusFooterText();
                expect (foot.containsIgnoreCase ("CPU"), foot);
                expect (foot.containsIgnoreCase ("SR") || foot.containsIgnoreCase ("BPM"), foot);
                expect (! foot.containsChar ((juce::juce_wchar) 0x00E2), foot);
            }
            ed->setWorkspaceMode (NeuroKoreAudioProcessorEditor::WorkspaceScript);
            expectEquals (ed->getWorkspaceMode(), (int) NeuroKoreAudioProcessorEditor::WorkspaceScript);
            expect (! ed->isEditingFormula(), "Script tab must open the live view, not force Edit");
            expect (ed->isLiveFormulaVisible());
            expect (! ed->isFormulaEditorVisible());
            ed.reset();
            proc.reset();
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        }

        beginTest ("formula display adds one inline IR button per slot");
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
            NeuroKoreAudioProcessor proc;
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
            const auto cropped = NeuroKoreLookAndFeel::cropOpaqueContent (src);
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

        beginTest ("graph canvas paints a no-out preset and survives a switch");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));

            const auto* shimmer = lib.findByName ("Shimmer Drive");
            const auto* wide = lib.findByName ("Wide Motion");
            expect (shimmer != nullptr);
            expect (wide != nullptr);
            if (shimmer == nullptr || wide == nullptr)
                return;

            expect (! wide->script.containsIgnoreCase ("\nout:"),
                    "Wide Motion must stay an out-less script for this crash case");

            NeuroKoreAudioProcessor proc;
            GraphCanvasComponent canvas (proc);
            canvas.setSize (900, 520);
            canvas.setScript (shimmer->script);
            expect (canvas.hasValidGraph());
            {
                juce::Image img (juce::Image::ARGB, 900, 520, true);
                juce::Graphics g (img);
                canvas.paintEntireComponent (g, false);
            }
            canvas.setScript (wide->script);
            expect (canvas.hasValidGraph(), canvas.getParseError());
            {
                juce::Image img (juce::Image::ARGB, 900, 520, true);
                juce::Graphics g (img);
                canvas.paintEntireComponent (g, false);
            }
        }

        beginTest ("popup menu type is large enough to read");
        {
            NeuroKoreLookAndFeel lf;
            expect (lf.getPopupMenuFont().getHeight() >= 17.f);
            int w = 0, h = 0;
            lf.getIdealPopupMenuItemSize ("Filter     low / high / band", false, 24, w, h);
            expect (h >= 28);
            expect (w >= 260);
        }

        beginTest ("DSL tokeniser colours comments and keywords");
        {
            expect (DslTokeniser::isKeyword ("stage1"));
            expect (DslTokeniser::isKeyword ("param"));
            expect (DslTokeniser::isKnobWord ("a"));
            expect (! DslTokeniser::isKnobWord ("drive"));
            juce::CodeDocument doc;
            doc.replaceAllContent ("# note\nstage1: y = a\n");
            DslTokeniser tok;
            juce::CodeDocument::Iterator it (doc);
            expectEquals (tok.readNextToken (it), (int) DslTokeniser::Comment);
            expectEquals (tok.readNextToken (it), (int) DslTokeniser::Keyword);
        }

        beginTest ("script error line extracts 1-based parser lines");
        {
            expectEquals (firstScriptErrorLine ("Error on line 12: bus needs a name"), 12);
            expectEquals (firstScriptErrorLine ("Malformed parameter line at 3"), 3);
            expectEquals (firstScriptErrorLine ("ok"), 0);
        }

        beginTest ("IR and preset tags do not use Apex middot");
        {
            expect (IrSlotUi::buttonText ("ir1", "Cab").contains ("ir1"));
            expect (IrSlotUi::buttonText ("ir1", "Cab").contains ("Cab"));
            expect (! IrSlotUi::buttonText ("ir1", "Cab").containsChar ((juce::juce_wchar) 0x00B7));
        }

        beginTest ("graph cards stay compact terminals plus a one-line caption");
        {
            expect (GraphCanvasComponent::kCardHeight <= 96);
            expect (GraphCanvasComponent::kCardWidth <= 220);
            expect (GraphCanvasComponent::kIoHeight <= 80);
            expect (GraphCanvasComponent::kIoWidth <= 96);
            expect (GraphCanvasComponent::kCardHeight >= 48);
            expectEquals (GraphCanvasComponent::formatLiveKnob (3.2f), juce::String ("3.20"));
            expectEquals (GraphCanvasComponent::formatLiveKnob (800.f), juce::String ("800"));
        }

        beginTest ("graph positions snap to the board grid");
        {
            expectEquals (GraphCanvasComponent::kGrid, 16);
            expectEquals (GraphCanvasComponent::snap (0), 0);
            expectEquals (GraphCanvasComponent::snap (7), 0);
            expectEquals (GraphCanvasComponent::snap (8), 16);
            expectEquals (GraphCanvasComponent::snap (9), 16);
            expectEquals (GraphCanvasComponent::snap (16), 16);
            expectEquals (GraphCanvasComponent::snap (24), 32);
            expectEquals (GraphCanvasComponent::snap (-7), 0);
            expectEquals (GraphCanvasComponent::snap (-8), -16);
            {
                NeuroKoreAudioProcessor proc;
                GraphCanvasComponent canvas (proc);
                canvas.setZoom (0.2f);
                expect (canvas.getZoom() >= GraphCanvasComponent::kZoomMin);
                canvas.setZoom (8.f);
                expect (canvas.getZoom() <= GraphCanvasComponent::kZoomMax);
            }
            const auto p = GraphCanvasComponent::snapPoint ({ 17, 31 });
            expectEquals (p.x, 16);
            expectEquals (p.y, 32);
        }

        beginTest ("graph load snaps @x,y comments onto the board grid");
        {
            NeuroKoreAudioProcessor proc;
            GraphCanvasComponent canvas (proc);
            canvas.setSize (900, 400);
            canvas.setScript ("stage1: y = x  # @17,31\n"
                              "filter1: type = lowpass; cutoff = 800  # @20,80");
            expect (canvas.hasValidGraph(), canvas.getParseError());
            const auto emitted = canvas.getEmittedScript();
            expect (emitted.contains ("@16.0,32.0"), emitted);
            expect (emitted.contains ("@16.0,80.0"), emitted);
        }

        beginTest ("factory apply tidies circuit positions");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            const auto* preset = lib.findByName ("Tape Echo Heads");
            expect (preset != nullptr);
            if (preset != nullptr)
            {
                NeuroKoreAudioProcessor proc;
                juce::String err;
                const int idx = (int) (preset - lib.getEntries().data());
                expect (lib.applyPreset (proc, idx, err), err);
                GraphCanvasComponent canvas (proc);
                canvas.setSize (900, 520);
                canvas.setScript (proc.getScript());
                expect (canvas.hasValidGraph(), canvas.getParseError());
                dsl::GraphDocument doc;
                expect (dsl::parse (canvas.getEmittedScript(), doc, err), err);
                expect (dsl::hasAllPositions (doc));
            }
        }

        beginTest ("factory load auto-arranges Trailer Impact at current zoom");
        {
            auto& lib = FactoryPresetLibrary::getInstance();
            if (lib.getEntries().empty())
                expect (lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)));
            const auto* preset = lib.findByName ("Trailer Impact");
            expect (preset != nullptr);
            if (preset != nullptr)
            {
                NeuroKoreAudioProcessor proc;
                juce::String err;
                const int idx = (int) (preset - lib.getEntries().data());
                expect (lib.applyPreset (proc, idx, err), err);
                GraphCanvasComponent canvas (proc);
                canvas.setSize (800, 620);
                canvas.setScript (proc.getScript());
                expect (canvas.hasValidGraph(), canvas.getParseError());
                dsl::GraphDocument doc;
                expect (dsl::parse (canvas.getEmittedScript(), doc, err), err);
                expect (dsl::hasAllPositions (doc));
                float maxX = 0.f, maxY = 0.f;
                float smashMinY = 1.0e9f, smashMaxY = -1.0e9f;
                int smashN = 0;
                for (const auto& n : doc.nodes)
                {
                    maxX = juce::jmax (maxX, n.x + (float) dsl::kTidyCardW);
                    maxY = juce::jmax (maxY, n.y + (float) dsl::kTidyCardH);
                    if (n.type != "out" && dsl::visualRail (n) == "smash")
                    {
                        ++smashN;
                        smashMinY = juce::jmin (smashMinY, n.y);
                        smashMaxY = juce::jmax (smashMaxY, n.y);
                    }
                }
                expect (smashN >= 5);
                expect (smashMaxY - smashMinY >= (float) dsl::kTidyCardH);
                expect (maxX <= 800.f + (float) dsl::kTidyMargin);
                expect (maxY <= 620.f + (float) dsl::kTidyMargin);
            }
        }

        beginTest ("circuit cable style defaults to dots");
        {
            const bool prev = UiSettings::get().cableWaveform();
            UiSettings::get().setCableWaveform (false);
            expect (! UiSettings::get().cableWaveform());
            UiSettings::get().setCableWaveform (true);
            expect (UiSettings::get().cableWaveform());
            UiSettings::get().setCableWaveform (prev);
        }

        beginTest ("circuit cable beads follow loudness, not sample peak");
        {
            expect (! GraphCanvasComponent::cableBeadsVisible (
                GraphCanvasComponent::loudnessToCableLevel (-100.f)));
            expect (! GraphCanvasComponent::cableBeadsVisible (
                GraphCanvasComponent::loudnessToCableLevel (-50.f)));
            expect (GraphCanvasComponent::cableBeadsVisible (
                GraphCanvasComponent::loudnessToCableLevel (-12.f)));
            expect (GraphCanvasComponent::loudnessToCableLevel (-18.f) > 0.2f);

            NeuroKoreAudioProcessor proc;
            proc.setPlayConfigDetails (2, 2, 48000.0, 128);
            proc.prepareToPlay (48000.0, 128);
            juce::AudioBuffer<float> buf (2, 128);
            juce::MidiBuffer midi;
            GraphCanvasComponent canvas (proc);
            canvas.setSize (900, 400);
            canvas.setScript ("stage1: y = x\n");
            expect (canvas.hasValidGraph(), canvas.getParseError());

            for (int b = 0; b < 20; ++b)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float s = 0.55f * std::sin (0.2f * (float) (b * 128 + i));
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                proc.processBlock (buf, midi);
                canvas.refreshCableMeters();
            }
            expect (GraphCanvasComponent::cableBeadsVisible (canvas.getCableInLevel()),
                    "loud inLevel=" + juce::String (canvas.getCableInLevel(), 4)
                    + " db=" + juce::String (proc.getLoudnessDb(), 1));

            for (int b = 0; b < 80; ++b)
            {
                buf.clear();
                proc.processBlock (buf, midi);
                canvas.refreshCableMeters();
            }
            expect (! GraphCanvasComponent::cableBeadsVisible (canvas.getCableInLevel()),
                    "silent inLevel=" + juce::String (canvas.getCableInLevel(), 4)
                    + " db=" + juce::String (proc.getLoudnessDb(), 1));
            proc.releaseResources();
        }

        beginTest ("circuit traces are ink, beads only when signal is present");
        {
            expect (! GraphCanvasComponent::cableBeadsVisible (0.f));
            expect (! GraphCanvasComponent::cableBeadsVisible (0.021f));
            expect (GraphCanvasComponent::cableBeadsVisible (0.05f));
            const auto idle = GraphCanvasComponent::cableTraceColour (false, 1.f);
            const auto lit = GraphCanvasComponent::cableTraceColour (true, 1.f);
            expect (idle.getPerceivedBrightness() > 0.7f);
            expect (lit.getPerceivedBrightness() > 0.85f);
            expect (idle.getSaturation() < 0.15f);
            expect (lit != NeuroKoreLookAndFeel::accent());
            const float silent[8] {};
            expect (GraphCanvasComponent::cableTapEnergy (silent, 8) < 0.01f);
            const float loud[8] { 0.8f, -0.6f, 0.4f, 0.f, 0.f, 0.f, 0.f, 0.f };
            expect (GraphCanvasComponent::cableTapEnergy (loud, 8) > 0.5f);
            expectEquals (GraphCanvasComponent::cableTapEnergy (nullptr, 8), 0.f);
        }

        beginTest ("lfo LED travels at constant speed; pulse follows hz");
        {
            expectEquals (GraphCanvasComponent::lfoChaseHz (0.f), 0.f);
            expectEquals (GraphCanvasComponent::lfoChaseHz (-2.f), 0.f);
            expect (GraphCanvasComponent::lfoChaseHz (0.01f) >= GraphCanvasComponent::kLfoChaseHzMin);
            expect (GraphCanvasComponent::lfoChaseHz (40.f) <= GraphCanvasComponent::kLfoChaseHzMax);
            expectEquals (GraphCanvasComponent::lfoChaseHz (2.f), 2.f);

            const float s1 = GraphCanvasComponent::lfoChaseStep (1.f, 30.f);
            const float s8 = GraphCanvasComponent::lfoChaseStep (8.f, 30.f);
            expect (s1 > 0.f);
            expect (s8 > s1);
            expectEquals (GraphCanvasComponent::lfoChaseStep (4.f, 0.f), 0.f);
            expectEquals (GraphCanvasComponent::lfoPulseAlpha (0.f), 0.f);
            expect (GraphCanvasComponent::lfoPulseAlpha (1.f) > 0.1f);
            expect (GraphCanvasComponent::lfoPulseAlpha (1.f) < 0.45f);

            expect (std::abs (GraphCanvasComponent::lfoChaseAlong (0.f, 100.f)) < 1.0e-5f);
            expect (GraphCanvasComponent::lfoChaseAlong (25.f, 100.f) > 0.2f);
            expect (std::abs (GraphCanvasComponent::lfoChaseAlong (100.f, 100.f)
                              - GraphCanvasComponent::lfoChaseAlong (0.f, 100.f)) < 1.0e-5f);

            const float p0 = GraphCanvasComponent::lfoLedPulse (2.f, 0.f);
            const float p1 = GraphCanvasComponent::lfoLedPulse (2.f, 0.125f);
            expect (std::abs (p0 - p1) > 0.2f);

            const float silent[8] {};
            expectEquals (GraphCanvasComponent::lfoChaseBrightness (silent, 8), 0.f);
            expectEquals (GraphCanvasComponent::lfoChaseBrightness (nullptr, 4), 0.f);
            const float deep[8] { 0.9f, -0.85f, 0.4f, 0.f, 0.f, 0.f, 0.f, 0.f };
            const float shallow[8] { 0.18f, -0.16f, 0.1f, 0.f, 0.f, 0.f, 0.f, 0.f };
            expect (GraphCanvasComponent::lfoChaseBrightness (deep, 8) > 0.8f);
            expect (GraphCanvasComponent::lfoChaseBrightness (shallow, 8) < 0.25f);
            expect (GraphCanvasComponent::lfoChaseBrightness (deep, 8)
                    > GraphCanvasComponent::lfoChaseBrightness (shallow, 8) * 2.f);
        }

        beginTest ("chip sizes and jack slots share the board grid");
        {
            expectEquals (GraphCanvasComponent::kGrid, 16);
            expectEquals (GraphCanvasComponent::kJackPitch, GraphCanvasComponent::kGrid);
            expectEquals (GraphCanvasComponent::kCardWidth % GraphCanvasComponent::kGrid, 0);
            expectEquals (GraphCanvasComponent::kCardHeight % GraphCanvasComponent::kGrid, 0);
            expectEquals (GraphCanvasComponent::kIoWidth % GraphCanvasComponent::kGrid, 0);
            expectEquals (GraphCanvasComponent::kIoHeight % GraphCanvasComponent::kGrid, 0);
            expectEquals (GraphCanvasComponent::jackLocalY (0),
                          GraphCanvasComponent::kTitleRows * GraphCanvasComponent::kGrid
                              + GraphCanvasComponent::kJackPad);
            expectEquals (GraphCanvasComponent::jackLocalY (1) - GraphCanvasComponent::jackLocalY (0),
                          GraphCanvasComponent::kGrid);
            expectEquals (GraphCanvasComponent::chipHeight (1, 1, 0, 0, false),
                          (GraphCanvasComponent::kTitleRows + 1 + GraphCanvasComponent::kBottomRows)
                              * GraphCanvasComponent::kGrid);
            expectEquals (GraphCanvasComponent::chipHeight (5, 2, 0, 0, false),
                          (GraphCanvasComponent::kTitleRows + 5 + GraphCanvasComponent::kBottomRows)
                              * GraphCanvasComponent::kGrid);
            expectEquals (GraphCanvasComponent::chipHeight (3, 2, 6, 1, true)
                              % GraphCanvasComponent::kGrid, 0);
            const auto fold = GraphCanvasComponent::foldChevronRect (208, 64, false, 1.f);
            expect (fold.getBottom() <= GraphCanvasComponent::kTitleRows * GraphCanvasComponent::kGrid + 2,
                    "chevron must stay in the title row");
            const auto knobPath = GraphCanvasComponent::makeKnobCable ({ 0.f, 20.f }, { 200.f, 80.f });
            juce::PathFlatteningIterator kit (knobPath);
            bool diagonal = false;
            while (kit.next())
            {
                const float dx = std::abs (kit.x2 - kit.x1);
                const float dy = std::abs (kit.y2 - kit.y1);
                if (dx > 1.f && dy > 1.f)
                    diagonal = true;
            }
            expect (! diagonal, "knob cable must be Manhattan, not a cubic");
            const int y0a = GraphCanvasComponent::jackLocalY (0);
            const int y0b = GraphCanvasComponent::jackLocalY (0);
            expectEquals (y0a, y0b);
            auto path = GraphCanvasComponent::makeOrthoCable ({ 10.f, 32.f }, { 200.f, 80.f });
            juce::PathFlatteningIterator it (path);
            while (it.next())
            {
                const float dx = std::abs (it.x2 - it.x1);
                const float dy = std::abs (it.y2 - it.y1);
                expect (dx < 0.6f || dy < 0.6f, "ortho cable must not go diagonal");
            }
        }

        beginTest ("effective BPM follows host or user setting");
        {
            const bool prevHost = UiSettings::get().useHostTempo();
            const float prevBpm = UiSettings::get().userBpm();
            NeuroKoreAudioProcessor proc;
            UiSettings::get().setUseHostTempo (true);
            expect (proc.isHostTempo());
            expect (proc.getEffectiveBpm() >= 20.f);
            expect (proc.getEffectiveBpm() <= 400.f);
            UiSettings::get().setUseHostTempo (false);
            UiSettings::get().setUserBpm (140.f);
            expect (! proc.isHostTempo());
            expectEquals (proc.getEffectiveBpm(), 140.f);
            UiSettings::get().setUseHostTempo (prevHost);
            UiSettings::get().setUserBpm (prevBpm);
        }

        beginTest ("circuit drag snap does not rewrite block order");
        {
            const juce::String script =
                "stage1: y = x  # @32,80\n"
                "filter1: type = lowpass; cutoff = 800  # @208,80\n";
            dsl::GraphDocument doc;
            juce::String err;
            expect (dsl::parse (script, doc, err), err);
            expectEquals ((int) doc.nodes.size(), 2);
            expectEquals (doc.nodes[0].name, juce::String ("stage1"));
            dsl::setPosition (doc, 0, 32.f, 208.f);
            expectEquals (doc.nodes[0].name, juce::String ("stage1"));
            expectEquals (doc.nodes[1].name, juce::String ("filter1"));
        }

        beginTest ("knob min/max bounds do not flip decimals with the live value");
        {
            expectEquals (ui::ParameterComponent::formatRangeBound (0.f), juce::String ("0"));
            expectEquals (ui::ParameterComponent::formatRangeBound (800.f), juce::String ("800"));
            expectEquals (ui::ParameterComponent::formatRangeBound (0.5f), juce::String ("0.50"));
            NeuroKoreAudioProcessor procRange;
            juce::String rangeErr;
            expect (procRange.setFormula ("param a = Drive [0, 2]\nstage1: y = x * a\n", rangeErr));
            const auto ranged = dsl::rewriteParamRange (procRange.getScript(), 0, 1.f, 8.f);
            expect (ranged.contains ("[1, 8]"));
            NeuroKoreAudioProcessor proc;
            ui::ParameterComponent pc (proc.apvts, "a", "Drive");
            pc.setSize (90, 120);
            pc.setMappedRange (0.f, 800.f);
            expectEquals (pc.getMinText(), juce::String ("MIN 0"));
            expectEquals (pc.getMaxText(), juce::String ("MAX 800"));
            pc.refreshValues();
            expectEquals (pc.getMaxText(), juce::String ("MAX 800"));
        }

        beginTest ("knob rename editor survives a value refresh");
        {
            NeuroKoreAudioProcessor proc;
            ui::ParameterComponent pc (proc.apvts, "a", "Drive");
            pc.setSize (90, 120);
            pc.startRename();
            expect (pc.isRenaming());
            pc.refreshValues();
            pc.setAliasName ("Other");
            expect (pc.isRenaming());
            expectEquals (pc.getDisplayedName(), juce::String ("Drive"));
        }

        beginTest ("formula and knob rename go on the undo stack");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("stage1: y = x\n", err));
            expect (proc.setFormula ("stage1: y = tanh(x)\n", err, false));
            expect (proc.undo());
            expect (proc.getScript().contains ("y = x"));
            expect (proc.redo());
            expect (proc.getScript().contains ("tanh"));
            const auto old = proc.getVariableName (0);
            proc.setVariableName (0, "drive");
            proc.recordNameChange (0, old, "drive");
            expectEquals (proc.getVariableName (0), juce::String ("drive"));
            expect (proc.undo());
            expectEquals (proc.getVariableName (0), old);
        }

        beginTest ("graph canvas has no add/remove toolbar");
        {
            NeuroKoreAudioProcessor proc;
            GraphCanvasComponent canvas (proc);
            canvas.setSize (800, 500);
            int buttons = 0;
            for (auto* c : canvas.getChildren())
                if (dynamic_cast<juce::TextButton*> (c) != nullptr)
                    ++buttons;
            expectEquals (buttons, 0);
            expectEquals (canvas.getNumChildComponents(), 1);
        }

        beginTest ("settings motion labels and persist Full/Reduced/Off");
        {
            expectEquals (SettingsUi::motionCaption (CyberMotion::Full), juce::String ("Full"));
            expectEquals (SettingsUi::motionCaption (CyberMotion::Reduced), juce::String ("Reduced"));
            expectEquals (SettingsUi::motionCaption (CyberMotion::Off), juce::String ("Off"));
            expect (SettingsUi::motionHint (CyberMotion::Off).containsIgnoreCase ("no motion"));
            expectEquals (juce::String (UiSettings::motionKey (CyberMotion::Full)), juce::String ("full"));
            expect (UiSettings::clampMotion (99) == CyberMotion::Full);
            expect (UiSettings::clampMotion ((int) CyberMotion::Reduced) == CyberMotion::Reduced);

            auto& s = UiSettings::get();
            const auto prevMotion = s.motion();
            const auto prevScale  = s.uiScalePercent();
            const auto prevFont   = s.editorFontPt();
            const auto prevLive   = s.liveMode();
            s.setMotion (CyberMotion::Reduced);
            expect (s.motion() == CyberMotion::Reduced);
            expect (! s.calmUi());
            s.setCalmUi (true);
            expect (s.motion() == CyberMotion::Off);
            expect (s.calmUi());
            s.setMotion (CyberMotion::Full);
            expect (! s.calmUi());
            s.setLiveMode (true);
            expect (s.liveMode());
            s.setLiveMode (false);
            expect (! s.liveMode());
            s.setMotion (prevMotion);
            s.setUiScalePercent (prevScale);
            s.setEditorFontPt (prevFont);
            s.setLiveMode (prevLive);
        }

        beginTest ("live mode IIR latency is far below studio FIR");
        {
            juce::dsp::Oversampling<float> fir (2, 2,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            juce::dsp::Oversampling<float> iir (2, 2,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
            fir.initProcessing (1024);
            iir.initProcessing (1024);
            const float firLat = fir.getLatencyInSamples();
            const float iirLat = iir.getLatencyInSamples();
            expect (iirLat < firLat * 0.5f,
                    "raw IIR " + juce::String (iirLat, 2) + " vs FIR " + juce::String (firLat, 2));

            auto& s = UiSettings::get();
            const auto prevLive = s.liveMode();
            NeuroKoreAudioProcessor proc;
            proc.prepareToPlay (48000.0, 64);
            proc.setLiveMode (false);
            const int studioOs = proc.getOversamplingLatencySamples();
            proc.setLiveMode (true);
            const int liveOs = proc.getOversamplingLatencySamples();
            expect (proc.isLiveMode());
            expect (liveOs < studioOs,
                    "live OS " + juce::String (liveOs) + " should be below studio OS " + juce::String (studioOs));
            expect (liveOs < 32, "live OS latency should stay under ~0.7 ms at 48 kHz");
            proc.setLiveMode (false);
            expect (! proc.isLiveMode());
            proc.releaseResources();
            s.setLiveMode (prevLive);
        }

        beginTest ("footer slots stay 13 pt and do not squash");
        {
            expect (StatusBarStrip::kFontPt >= 13.f);
            expect (Config::kFooterRowHeight >= 26);
            expect (BrandLockup::kMinTitlePt >= 14.f);
            expect (BrandLockup::kMinVersionPt >= 12.f);
            expect (SettingsUi::motionHint (CyberMotion::Reduced).containsIgnoreCase ("SNAP"));
        }

        beginTest ("function plots distinguish OTT, widen, octaver, vocoder");
        {
            using K = FunctionPlotComponent::PlotKind;
            expect (FunctionPlotComponent::kindForName ("ott") == K::Ott);
            expect (FunctionPlotComponent::kindForName ("widen") == K::Widen);
            expect (FunctionPlotComponent::kindForName ("octaver") == K::Octaver);
            expect (FunctionPlotComponent::kindForName ("vocoder") == K::Vocoder);
            expect (FunctionPlotComponent::kindForName ("softclip") == K::Transfer);
            expect (FunctionPlotComponent::kindForName ("ott")
                    != FunctionPlotComponent::kindForName ("widen"));
            expect (FunctionPlotComponent::kindForName ("octaver")
                    != FunctionPlotComponent::kindForName ("vocoder"));
            FunctionPlotComponent plot;
            plot.setFunctionDemo ("ott", "ott1: depth = 0.4");
            expect (plot.kind() == K::Ott);
            expect (plot.caption().containsIgnoreCase ("OTT"));
            plot.setFunctionDemo ("widen", "widen1: width = 0.45");
            expect (plot.kind() == K::Widen);
            expect (plot.caption().containsIgnoreCase ("Widen"));
        }

        beginTest ("stages window reorders two filters with Up");
        {
            NeuroKoreAudioProcessor proc;
            juce::String err;
            expect (proc.setFormula ("filter1: type = lowpass; cutoff = 800\n"
                                     "filter2: type = highpass; cutoff = 120\n", err), err);
            StagesContentComponent stages (proc);
            expect (stages.rowCount() >= 2);
            expectEquals (stages.rowName (0), juce::String ("filter1"));
            expect (stages.moveSelected (1));
            expectEquals (stages.rowName (0), juce::String ("filter2"));
            expectEquals (stages.rowName (1), juce::String ("filter1"));
        }
    }
};

#endif
