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

        // --- Context: type = ...
        if (ctx == "type" || ctx.endsWithIgnoreCase ("type=") || ctx == "=")
        {
            // Narrow: only when previous word is type or line has "type"
            const bool afterType = head.contains ("type") || ctx.contains ("type");
            if (afterType)
            {
                addAll (items, { "lowpass", "highpass", "bandpass",
                                 "sine", "saw", "triangle", "square",
                                 "peak", "rms", "encode", "decode" },
                        Kind::Value, "type value", prefix);
            }
        }

        if (head.contains ("channel") || ctx == "channel" || ctx.contains ("channel="))
            addAll (items, { "left", "right", "both", "mid", "side" },
                    Kind::Value, "channel", prefix);

        if (head.contains ("mode") || ctx == "mode" || ctx.contains ("mode="))
            addAll (items, { "encode", "decode" }, Kind::Value, "ms mode", prefix);

        if (head.contains ("pingpong") || ctx == "pingpong")
            addAll (items, { "true", "false" }, Kind::Value, "bool", prefix);

        // --- After block name "filter1:" properties
        const bool inBlockProps = head.contains (":") && ! head.contains ("y =") && ! head.contains ("y=");
        if (inBlockProps || forceAll)
        {
            juce::StringArray props;
            if (head.startsWith ("filter") || head.contains ("filter") || forceAll)
                props.addArray ({ "type", "cutoff", "resonance", "center", "width",
                                  "lowcut", "highcut", "channel" });
            if (head.startsWith ("comp") || forceAll)
                props.addArray ({ "threshold", "ratio", "attack", "release" });
            if (head.startsWith ("delay") || forceAll)
                props.addArray ({ "time", "feedback", "mix", "damp", "sync", "pingpong" });
            if (head.startsWith ("reverb") || forceAll)
                props.addArray ({ "size", "decay", "damp", "mix", "width" });
            if (head.startsWith ("osc") || forceAll)
                props.addArray ({ "type", "freq", "sync" });
            if (head.startsWith ("env") || forceAll)
                props.addArray ({ "type", "attack", "release" });
            if (head.startsWith ("ms") || forceAll)
                props.addArray ({ "mode" });
            if (head.startsWith ("stage") || forceAll)
                props.addArray ({ "channel", "y" });
            props.removeDuplicates (false);
            addAll (items, props, Kind::Property, "property", prefix, " = ");
        }

        // --- Line-start block / keyword
        if (isIdentStartLine (head) || forceAll)
        {
            addAll (items, {
                "param", "stage", "filter", "comp", "osc", "env",
                "delay", "reverb", "ms", "bus", "send", "out", "in", "main"
            }, Kind::Block, "block", prefix);

            // Snippets for common blocks
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
            if (prefix.isEmpty() || juce::String ("param").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "param a = Name [min, max]";
                sn.insertText = "param a = Name [0.0, 1.0]";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
            if (prefix.isEmpty() || juce::String ("bus").startsWithIgnoreCase (prefix))
            {
                Item sn;
                sn.label = "bus dirt + send in";
                sn.insertText = "bus dirt:\nsend: in = 1\nstage2: y = x\nout: main = 1; dirt = 1";
                sn.detail = "snippet";
                sn.kind = Kind::Snippet;
                addCand (items, std::move (sn), prefix);
            }
        }

        // --- Expression context (stage y = ...): functions + knobs + globals
        const bool inExpr = head.contains ("y =") || head.contains ("y=")
                         || head.contains ("cutoff") || head.contains ("freq")
                         || head.contains ("(") || forceAll
                         || (! inBlockProps && ! isIdentStartLine (head) && prefix.isNotEmpty());

        if (inExpr || prefix.isNotEmpty() || forceAll)
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
                    al.insertText = letter; // insert canonical knob letter
                    al.detail = "alias → " + letter;
                    al.kind = Kind::Variable;
                    addCand (items, std::move (al), prefix);
                }
            }

            addAll (items, {
                "x", "y", "x_prev", "y_prev", "t", "sr", "pi", "ch",
                "midi_note", "midi_freq", "midi_vel", "midi_gate", "midi_bend", "midi_mod"
            }, Kind::Variable, "signal", prefix);

            addAll (items, { "map", "lerp", "softclip", "hardclip", "tube", "diode" },
                    Kind::Function, "common", prefix, "(");
        }

        // Deduplicate by label (keep best score)
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

        // Don't show list with empty prefix unless forced or strong context
        if (prefix.isEmpty() && ! forceAll && unique.size() > 12)
            unique.resize (12);

        if (prefix.isEmpty() && ! forceAll
            && ! ctx.contains ("type") && ! ctx.contains ("channel")
            && ! ctx.contains ("mode") && ! inBlockProps)
        {
            // Free typing: only show if user already started a word
            return {};
        }

        return unique;
    }
}
