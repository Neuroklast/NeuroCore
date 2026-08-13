#pragma once
#include <JuceHeader.h>

/** Token search + DSL/name tag inference for the preset explorer. */
namespace PresetSearch
{
inline juce::String normalize (const juce::String& s)
{
    return s.toLowerCase().replaceCharacters ("-_/", "   ");
}

inline juce::StringArray tokenizeQuery (const juce::String& query)
{
    juce::StringArray tokens;
    tokens.addTokens (normalize (query), " \t,", "");
    tokens.trim();
    tokens.removeEmptyStrings();
    return tokens;
}

inline bool containsToken (const juce::String& hay, const juce::String& tok)
{
    if (tok.isEmpty())
        return true;
    if (tok.length() >= 4)
        return hay.contains (tok);

    int i = 0;
    while ((i = hay.indexOf (i, tok)) >= 0)
    {
        const auto before = (i == 0) ? (juce_wchar) ' '
                                     : hay[i - 1];
        const int afterIdx = i + tok.length();
        const auto after = (afterIdx >= hay.length()) ? (juce_wchar) ' '
                                                      : hay[afterIdx];
        const auto letter = [] (juce_wchar c)
        {
            return juce::CharacterFunctions::isLetterOrDigit (c);
        };
        if (! letter (before) && ! letter (after))
            return true;
        ++i;
    }
    return false;
}

inline void addTag (juce::StringArray& tags, juce::String t)
{
    t = t.trim().toLowerCase();
    if (t.isNotEmpty() && ! tags.contains (t, true))
        tags.add (t);
}

inline juce::StringArray inferTags (const juce::String& script,
                                    const juce::String& name = {},
                                    const juce::String& category = {},
                                    const juce::String& description = {})
{
    juce::StringArray tags;
    const auto blob = (script + "\n" + name + "\n" + description + "\n" + category).toLowerCase();
    const auto hay = normalize (blob);
    auto has = [&] (const char* p) { return blob.contains (p); };
    auto hasWord = [&] (const char* p) { return containsToken (hay, p); };

    if (has ("delay"))
        addTag (tags, "delay");
    if (has ("reverb") || has ("verb1") || has ("verb2") || has ("verb3"))
    {
        addTag (tags, "reverb");
        addTag (tags, "space");
    }
    if (has ("ms1") || has ("ms2") || has ("ms3") || has ("ms4")
        || has ("mode = encode") || has ("mode = decode")
        || has ("channel = mid") || has ("channel = side")
        || has ("ms_encode") || has ("ms_decode"))
    {
        addTag (tags, "mid-side");
        addTag (tags, "midside");
        addTag (tags, "ms");
        addTag (tags, "mid");
        addTag (tags, "side");
    }
    if (has ("bus ") || has ("bus\n") || has ("bus:") || has ("send:") || has ("out:"))
    {
        addTag (tags, "bus");
        addTag (tags, "parallel");
    }
    if (has ("tube"))
        addTag (tags, "tube");
    if (has ("softclip") || has ("hardclip"))
        addTag (tags, "clip");
    if (has ("hardclip"))
        addTag (tags, "hardclip");
    if (has ("bitcrush"))
    {
        addTag (tags, "bitcrush");
        addTag (tags, "lo-fi");
    }
    if (has ("fold"))
        addTag (tags, "fold");
    if (has ("diode"))
        addTag (tags, "diode");
    if (has ("comp"))
        addTag (tags, "compressor");
    if (has ("filter"))
        addTag (tags, "filter");
    if (has ("lowpass"))
        addTag (tags, "lowpass");
    if (has ("highpass"))
        addTag (tags, "highpass");
    if (has ("pingpong"))
    {
        addTag (tags, "pingpong");
        addTag (tags, "stereo");
    }
    if (has ("osc"))
    {
        addTag (tags, "modulation");
        addTag (tags, "lfo");
    }
    if (has ("env"))
        addTag (tags, "envelope");

    static constexpr const char* kWords[] = {
        "tape", "crunch", "vocal", "drum", "kick", "snare", "hat",
        "bass", "guitar", "amp", "fuzz", "overdrive", "chorus", "phaser",
        "tremolo", "shimmer", "hall", "plate", "slap", "glue", "air",
        "width", "mono", "room", "master", "crush", "lofi", "edm",
        "synth", "pad", "lead", "send", "drive", "saturate", "clipper",
        "haas", "cinematic", "trailer", "score", "dialogue", "boom", "impact"
    };
    for (auto* w : kWords)
        if (hasWord (w))
            addTag (tags, w);

    if (category.isNotEmpty())
        addTag (tags, category);
    return tags;
}

inline juce::StringArray mergeTags (juce::StringArray explicitTags,
                                    const juce::StringArray& inferred)
{
    for (const auto& t : inferred)
        addTag (explicitTags, t);
    explicitTags.sortNatural();
    return explicitTags;
}

inline juce::String buildHaystack (const juce::String& name,
                                   const juce::String& category,
                                   const juce::String& description,
                                   const juce::String& author,
                                   const juce::StringArray& tags,
                                   const juce::String& script)
{
    juce::String h;
    h << name << " " << category << " " << description << " " << author
      << " " << tags.joinIntoString (" ") << " " << script;
    return h;
}

inline bool matches (const juce::String& haystack, const juce::String& query)
{
    const auto q = query.trim();
    if (q.isEmpty())
        return true;
    const auto hay = normalize (haystack);
    if (hay.contains (normalize (q)))
        return true;
    for (const auto& tok : tokenizeQuery (q))
        if (! containsToken (hay, tok))
            return false;
    return true;
}
} // namespace PresetSearch
