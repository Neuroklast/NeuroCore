#pragma once

#include <JuceHeader.h>
#include "PluginLookAndFeel.h"

/** Red / white / black colouring for the DSL script editor. */
class DslTokeniser : public juce::CodeTokeniser
{
public:
    enum Token
    {
        Unknown = 0,
        Comment,
        Keyword,
        Number,
        Operator,
        Knob,
        Identifier
    };

    static bool isKeyword (const juce::String& w)
    {
        static const char* k[] = {
            "param", "stage", "filter", "eq", "comp", "gate", "limit", "limiter",
            "delay", "reverb", "ir", "ott", "widen", "osc", "env", "bus", "send",
            "out", "ms", "vocoder", "octaver", "xover", "convolve", nullptr
        };
        const auto s = w.toLowerCase();
        for (int i = 0; k[i] != nullptr; ++i)
        {
            const juce::String key (k[i]);
            if (s == key)
                return true;
            if (s.startsWith (key)
                && s.substring (key.length()).containsOnly ("0123456789"))
                return true;
        }
        return false;
    }

    static bool isKnobWord (const juce::String& w)
    {
        return w.length() == 1
            && w[0] >= 'a' && w[0] <= 'f';
    }

    int readNextToken (juce::CodeDocument::Iterator& it) override
    {
        it.skipWhitespace();
        const auto c = it.peekNextChar();
        if (c == 0)
            return Unknown;

        if (c == '#')
        {
            it.skipToEndOfLine();
            return Comment;
        }

        if (c == '/')
        {
            it.skip();
            if (it.peekNextChar() == '/')
            {
                it.skipToEndOfLine();
                return Comment;
            }
            return Operator;
        }

        if (juce::CharacterFunctions::isDigit (c)
            || (c == '.' && juce::CharacterFunctions::isDigit (it.peekNextChar())))
        {
            if (c == '.')
                it.skip();
            while (juce::CharacterFunctions::isDigit (it.peekNextChar())
                   || it.peekNextChar() == '.' || it.peekNextChar() == 'e'
                   || it.peekNextChar() == 'E' || it.peekNextChar() == '-')
                it.skip();
            return Number;
        }

        if (juce::CharacterFunctions::isLetter (c) || c == '_')
        {
            juce::String w;
            w += it.nextChar();
            while (juce::CharacterFunctions::isLetterOrDigit (it.peekNextChar())
                   || it.peekNextChar() == '_')
                w += it.nextChar();
            if (isKeyword (w))
                return Keyword;
            if (isKnobWord (w))
                return Knob;
            return Identifier;
        }

        it.skip();
        return Operator;
    }

    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override
    {
        juce::CodeEditorComponent::ColourScheme scheme;
        scheme.types.add ({ "Unknown",    NeuroKoreLookAndFeel::ink() });
        scheme.types.add ({ "Comment",    NeuroKoreLookAndFeel::inkMuted() });
        scheme.types.add ({ "Keyword",    NeuroKoreLookAndFeel::accent() });
        scheme.types.add ({ "Number",     NeuroKoreLookAndFeel::ink() });
        scheme.types.add ({ "Operator",   NeuroKoreLookAndFeel::inkMuted() });
        scheme.types.add ({ "Knob",       NeuroKoreLookAndFeel::accent() });
        scheme.types.add ({ "Identifier", NeuroKoreLookAndFeel::ink() });
        return scheme;
    }
};
