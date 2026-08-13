#ifndef EDITORUXTEST_H
#define EDITORUXTEST_H

#include <JuceHeader.h>
#include <array>
#include "../src/ui/HelpContentComponent.h"
#include "../src/ui/DslAutocomplete.h"
#include "../src/core/Config.h"
#include "../src/core/EffectParameters.h"

class EditorUxTest : public juce::UnitTest
{
public:
    EditorUxTest() : juce::UnitTest ("EditorUxTest", "UI") {}

    void runTest() override
    {
        beginTest ("Help parser splits ## chapters");
        {
            const juce::String md =
                "# Title\n\nintro\n\n## 1. Quickstart\nDo this.\n\n## 2. Knobs\nTurn a.\n";
            const auto ch = parseHelpChapters (md);
            expectEquals ((int) ch.size(), 2);
            expect (ch[0].title.contains ("Quickstart"));
            expect (ch[1].title.contains ("Knobs"));
            expect (ch[0].body.contains ("Do this"));
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
