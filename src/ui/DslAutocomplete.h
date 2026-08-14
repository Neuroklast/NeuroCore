#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST

    Context-aware DSL completion for the formula editor (IDE-style).
*/

#include <JuceHeader.h>
#include "../utils/FormulaHelper.h"
#include "../core/Config.h"
#include <vector>
#include <algorithm>
#include <array>

namespace DslAutocomplete
{
    enum class Kind
    {
        Keyword,
        Block,
        Property,
        Value,
        Function,
        Variable,
        Snippet
    };

    struct Item
    {
        juce::String label;      ///< Insert / display name
        juce::String insertText; ///< What to insert (may include "()")
        juce::String detail;     ///< Right-hand hint
        Kind kind { Kind::Keyword };
        int score { 0 };         ///< Higher = better match
    };

    inline juce::String kindTag (Kind k) noexcept
    {
        switch (k)
        {
            case Kind::Keyword:  return "kw";
            case Kind::Block:    return "block";
            case Kind::Property: return "prop";
            case Kind::Value:    return "val";
            case Kind::Function: return "fn";
            case Kind::Variable: return "var";
            case Kind::Snippet:  return "snip";
        }
        return {};
    }

    /** Token [start, caret) that is being completed. */
    inline void wordAt (const juce::String& text, int caret, int& start, juce::String& prefix)
    {
        caret = juce::jlimit (0, text.length(), caret);
        start = caret;
        while (start > 0)
        {
            const auto c = text[start - 1];
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_' || c == '.')
                --start;
            else
                break;
        }
        prefix = text.substring (start, caret);
    }

    /** Non-whitespace text immediately before `start`. */
    inline juce::String contextBefore (const juce::String& text, int start)
    {
        int i = start;
        while (i > 0 && juce::CharacterFunctions::isWhitespace (text[i - 1]))
            --i;
        int j = i;
        while (j > 0)
        {
            const auto c = text[j - 1];
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_' || c == '=' || c == ':' || c == '.')
                --j;
            else
                break;
        }
        return text.substring (j, i).trim().toLowerCase();
    }

    /** Current line without the incomplete token. */
    inline juce::String lineHead (const juce::String& text, int start)
    {
        int lineStart = start;
        while (lineStart > 0 && text[lineStart - 1] != '\n')
            --lineStart;
        return text.substring (lineStart, start).trim().toLowerCase();
    }

    inline bool isIdentStartLine (const juce::String& head)
    {
        // Empty or only "filter1" style incomplete id at beginning
        return head.isEmpty()
            || head.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789_");
    }

    inline juce::String lineBlockKind (const juce::String& head)
    {
        auto first = head.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
        if (first.startsWith ("eq")) return "eq";
        if (first.startsWith ("octaver") || first == "octave") return "octaver";
        if (first.startsWith ("vocoder")) return "vocoder";
        if (first.startsWith ("filter") || first == "lpf" || first == "hpf" || first == "bpf"
            || first == "lp" || first == "hp" || first == "bp"
            || first == "lowpass" || first == "highpass" || first == "bandpass")
            return "filter";
        if (first.startsWith ("comp")) return "comp";
        if (first.startsWith ("gate")) return "gate";
        if (first.startsWith ("limit")) return "limit";
        if (first.startsWith ("xover") || first.startsWith ("crossover")) return "xover";
        if (first.startsWith ("ir") || first.startsWith ("convolve")) return "ir";
        if (first.startsWith ("delay")) return "delay";
        if (first.startsWith ("reverb") || first.startsWith ("verb")) return "reverb";
        if (first.startsWith ("osc")) return "osc";
        if (first.startsWith ("env")) return "env";
        if (first.startsWith ("ms") || first.startsWith ("midside")) return "ms";
        if (first.startsWith ("stage")) return "stage";
        if (first.startsWith ("param")) return "param";
        if (first.startsWith ("bus")) return "bus";
        if (first == "send") return "send";
        if (first == "out") return "out";
        return {};
    }

    inline bool canSuggestSend (const juce::String& text, int caret)
    {
        const auto before = text.substring (0, juce::jlimit (0, text.length(), caret));
        int lastBus = before.lastIndexOfIgnoreCase ("\nbus ");
        if (before.startsWithIgnoreCase ("bus "))
            lastBus = 0;
        const int lastOut = before.lastIndexOfIgnoreCase ("\nout");
        return lastBus >= 0 && lastBus > lastOut;
    }

    inline bool canSuggestOut (const juce::String& text, int caret)
    {
        const auto before = text.substring (0, juce::jlimit (0, text.length(), caret));
        return ! before.containsIgnoreCase ("\nout:") && ! before.startsWithIgnoreCase ("out:");
    }

    inline std::vector<Item> uniqueSorted (std::vector<Item> items,
                                           const juce::String& prefix,
                                           bool forceAll,
                                           const juce::String& ctx,
                                           bool inBlockProps)
    {
        std::sort (items.begin(), items.end(),
                   [] (const Item& a, const Item& b)
                   {
                       if (a.score != b.score) return a.score > b.score;
                       return a.label < b.label;
                   });
        std::vector<Item> unique;
        unique.reserve (items.size());
        for (auto& it : items)
        {
            bool dup = false;
            for (auto& u : unique)
                if (u.label.equalsIgnoreCase (it.label))
                {
                    dup = true;
                    break;
                }
            if (! dup)
                unique.push_back (std::move (it));
            if ((int) unique.size() >= 24)
                break;
        }

        if (prefix.isEmpty() && ! forceAll && unique.size() > 12)
            unique.resize (12);

        if (prefix.isEmpty() && ! forceAll
            && ! ctx.contains ("type") && ! ctx.contains ("channel")
            && ! ctx.contains ("mode") && ! inBlockProps)
        {
            return {};
        }
        return unique;
    }

    inline void addCand (std::vector<Item>& out, Item item, const juce::String& prefix)
    {
        const auto p = prefix.toLowerCase();
        const auto l = item.label.toLowerCase();
        if (p.isNotEmpty())
        {
            if (l.startsWith (p))
                item.score = 1000 - (l.length() - p.length());
            else if (l.contains (p))
                item.score = 400 - l.indexOf (p);
            else
                return; // no match when filtering
        }
        else
        {
            item.score = 100;
        }
        out.push_back (std::move (item));
    }

    inline void addAll (std::vector<Item>& out, const juce::StringArray& labels,
                        Kind kind, const juce::String& detail,
                        const juce::String& prefix, const juce::String& insertSuffix = {})
    {
        for (const auto& lab : labels)
        {
            Item it;
            it.label = lab;
            it.insertText = lab + insertSuffix;
            it.detail = detail;
            it.kind = kind;
            addCand (out, std::move (it), prefix);
        }
    }

    /**
        Build ranked completions for caret position.
        @param forceAll  Ctrl+Space: show broad list even with empty prefix in free context
    */
    inline std::vector<Item> complete (const juce::String& text,
                                       int caret,
                                       const std::array<juce::String, Config::kNumUserParams>& varNames,
                                       bool forceAll = false)
    {
        int start = 0;
        juce::String prefix;
        wordAt (text, caret, start, prefix);

        const auto ctx = contextBefore (text, start);
        const auto head = lineHead (text, start);
        std::vector<Item> items;

        const auto kind = lineBlockKind (head);
        const bool afterType = head.contains ("type") && (ctx == "=" || ctx.contains ("type"));
        const bool afterChannel = head.contains ("channel") && (ctx == "=" || ctx.contains ("channel"));
        const bool afterMode = head.contains ("mode") && (ctx == "=" || ctx.contains ("mode"));
        const bool afterPingpong = head.contains ("pingpong") && (ctx == "=" || ctx.contains ("pingpong"));
        const bool afterEquals = ctx == "=" || head.contains ("=");
        const bool inBlockProps = head.contains (":") && ! head.contains ("y =") && ! head.contains ("y=");

        if (afterType)
        {
            if (kind == "filter")
                addAll (items, { "lowpass", "highpass", "bandpass" }, Kind::Value, "filter type", prefix);
            else if (kind == "eq")
                addAll (items, { "peak", "notch", "lowcut", "highcut", "lowshelf", "highshelf" },
                        Kind::Value, "eq type", prefix);
            else if (kind == "osc")
                addAll (items, { "sine", "saw", "triangle", "square" }, Kind::Value, "osc type", prefix);
            else if (kind == "env")
                addAll (items, { "peak", "rms" }, Kind::Value, "env type", prefix);
            return uniqueSorted (items, prefix, forceAll, ctx, inBlockProps);
        }

        if (afterChannel)
        {
            addAll (items, { "left", "right", "both", "mid", "side" }, Kind::Value, "channel", prefix);
            return uniqueSorted (items, prefix, forceAll, ctx, inBlockProps);
        }

        if (afterMode)
        {
            addAll (items, { "encode", "decode" }, Kind::Value, "ms mode", prefix);
            return uniqueSorted (items, prefix, forceAll, ctx, inBlockProps);
        }

        if (afterPingpong)
        {
            addAll (items, { "true", "false" }, Kind::Value, "bool", prefix);
            return uniqueSorted (items, prefix, forceAll, ctx, inBlockProps);
        }

        if (inBlockProps && ! afterEquals)
        {
            juce::StringArray props;
            if (kind == "filter")
                props.addArray ({ "type", "cutoff", "resonance", "center", "width",
                                  "lowcut", "highcut", "channel" });
            else if (kind == "eq")
                props.addArray ({ "type", "freq", "q", "gain", "channel" });
            else if (kind == "octaver")
                props.addArray ({ "sub", "up", "mix", "tone", "thresh" });
            else if (kind == "vocoder")
                props.addArray ({ "bands", "mix", "q", "formant", "dry" });
            else if (kind == "comp")
                props.addArray ({ "threshold", "ratio", "attack", "release",
                                  "knee", "makeup", "hpf", "source" });
            else if (kind == "gate")
                props.addArray ({ "threshold", "hyst", "attack", "hold", "release", "range", "source" });
            else if (kind == "limit")
                props.addArray ({ "ceiling", "release" });
            else if (kind == "xover")
                props.addArray ({ "f1", "f2" });
            else if (kind == "ir")
                props.addArray ({ "mix", "gain" });
            else if (kind == "env")
                props.addArray ({ "type", "attack", "release", "source", "trigger" });
            else if (kind == "delay")
                props.addArray ({ "time", "feedback", "mix", "damp", "sync", "pingpong" });
            else if (kind == "reverb")
                props.addArray ({ "size", "decay", "damp", "mix", "width" });
            else if (kind == "osc")
                props.addArray ({ "type", "freq", "sync" });
            else if (kind == "ms")
                props.addArray ({ "mode" });
            else if (kind == "stage")
                props.addArray ({ "channel", "y" });
            else if (kind == "send")
                props.addArray ({ "in" });
            else if (kind == "out")
                props.addArray ({ "main" });
            addAll (items, props, Kind::Property, "property", prefix, " = ");
            return uniqueSorted (items, prefix, forceAll, ctx, inBlockProps);
        }

        if (isIdentStartLine (head))
        {
            juce::StringArray blocks { "param", "stage", "filter", "eq", "comp", "gate", "limit", "osc", "env",
                                       "delay", "reverb", "ms", "octaver", "vocoder", "xover", "ir", "bus" };
            if (canSuggestSend (text, start))
                blocks.add ("send");
            if (canSuggestOut (text, start))
                blocks.add ("out");
            addAll (items, blocks, Kind::Block, "block", prefix);

            if (prefix.isEmpty() || juce::String ("stage").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "stageN: y = ...";
                sn.insertText = "stage1: y = ";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("filter").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "filter lowpass";
                sn.insertText = "filter1: type = lowpass; cutoff = 1000; resonance = 0.4";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("eq").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "eq peak/notch/cut";
                sn.insertText = "eq1: type = peak; freq = 1000; q = 1.2; gain = 3";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("octaver").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "octaver sub/up";
                sn.insertText = "octaver1: sub = 0.65; up = 0.2; mix = 0.72; tone = 420";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("vocoder").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "vocoder (sidechain = voice)";
                sn.insertText = "vocoder1: bands = 8; mix = 0.85; q = 2.2; formant = 1; dry = 0.15";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("gate").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "gate (hysteresis)";
                sn.insertText = "gate1: threshold = -42; hyst = 3; attack = 0.001; hold = 0.04; release = 0.08; range = -70";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("limit").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "limit (ceiling)";
                sn.insertText = "limit1: ceiling = -0.3; release = 0.08";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("xover").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "xover 3-band";
                sn.insertText = "xover1: f1 = 120; f2 = 2500";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("ir").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "ir (cab / room)";
                sn.insertText = "ir1: mix = 1; gain = 0";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("param").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "param a = Name [min, max]";
                sn.insertText = "param a = Name [0.0, 1.0]";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);

                Item note;
                note.label = "param a = Time [1/1, 1/16]";
                note.insertText = "param a = Time [1/1, 1/16]";
                note.detail = "snippet";
                note.kind = Kind::Snippet;
                addCand (items, std::move (note), prefix);
            }
            if ((prefix.isEmpty() || juce::String ("bus").startsWithIgnoreCase (prefix))
                && canSuggestSend (text, start) == false)
            {
                Item sn;
                sn.label = "bus dirt + send in";
                sn.insertText = "bus dirt:\nsend: in = 1\nstage2: y = x\nout: main = 1; dirt = 1";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            return uniqueSorted (items, prefix, forceAll, ctx, inBlockProps);
        }

        const bool inExpr = head.contains ("y =") || head.contains ("y=")
                         || afterEquals
                         || head.contains ("(");

        if (inExpr)
        {
            for (const auto& f : builtinFunctions)
            {
                Item it;
                it.label = f;
                it.insertText = f + "(";
                it.detail = "function";
                it.kind = Kind::Function;
                addCand (items, std::move (it), prefix);
            }

            for (int i = 0; i < Config::kNumUserParams; ++i)
            {
                const juce::String letter (Config::kDefaultVariableNames[i]);
                Item it;
                it.label = letter;
                it.insertText = letter;
                it.detail = varNames[(size_t) i].isNotEmpty()
                                ? varNames[(size_t) i] : "knob";
                it.kind = Kind::Variable;
                addCand (items, std::move (it), prefix);

                if (varNames[(size_t) i].isNotEmpty()
                    && varNames[(size_t) i] != letter)
                {
                    Item al;
                    al.label = varNames[(size_t) i];
                    al.insertText = letter;
                    al.detail = "alias -> " + letter;
                    al.kind = Kind::Variable;
                    addCand (items, std::move (al), prefix);
                }
            }

            addAll (items, {
                "x", "y", "x_prev", "y_prev", "t", "sr", "pi", "ch",
                "midi_note", "midi_freq", "midi_vel", "midi_gate", "midi_bend", "midi_mod",
                "sc", "sc_l", "sc_r", "sidechain"
            }, Kind::Variable, "signal", prefix);
        }

        return uniqueSorted (std::move (items), prefix, forceAll, ctx, inBlockProps);
    }
}
