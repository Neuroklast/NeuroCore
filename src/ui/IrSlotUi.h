#pragma once

#include <JuceHeader.h>
#include <vector>
#include "PluginLookAndFeel.h"

/** Shared IR-slot helpers for the formula view and the code editor. */
namespace IrSlotUi
{
inline bool isIrSlotId (const juce::String& id) noexcept
{
    const auto t = id.trim().toLowerCase();
    if (t == "ir" || t == "convolve")
        return true;
    if (t.startsWith ("ir") && t.length() > 2
        && t.substring (2).containsOnly ("0123456789"))
        return true;
    if (t.startsWith ("convolve") && t.length() > 8
        && t.substring (8).containsOnly ("0123456789"))
        return true;
    return false;
}

inline juce::String slotFromLine (const juce::String& line)
{
    auto t = line.trimStart();
    if (t.startsWithChar ('#') || t.startsWith ("//"))
        return {};
    const int colon = t.indexOfChar (':');
    if (colon <= 0)
        return {};
    const auto id = t.substring (0, colon).trim().toLowerCase();
    return isIrSlotId (id) ? id : juce::String();
}

inline void collectSlots (const juce::String& script,
                          std::vector<juce::String>& slots,
                          std::vector<int>& lines)
{
    slots.clear();
    lines.clear();
    juce::StringArray rows;
    rows.addLines (script);
    for (int i = 0; i < rows.size(); ++i)
    {
        const auto s = slotFromLine (rows[i]);
        if (s.isNotEmpty())
        {
            slots.push_back (s);
            lines.push_back (i);
        }
    }
}

inline void styleButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, NeuroKoreLookAndFeel::surface());
    b.setColour (juce::TextButton::buttonOnColourId, NeuroKoreLookAndFeel::surfaceHigh());
    b.setColour (juce::TextButton::textColourOffId, NeuroKoreLookAndFeel::ink());
    b.setColour (juce::TextButton::textColourOnId, NeuroKoreLookAndFeel::ink());
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    b.setWantsKeyboardFocus (false);
}

inline juce::String buttonText (const juce::String& slot, const juce::String& caption)
{
    return slot + " / " + (caption.isNotEmpty() ? caption : juce::String ("IR"));
}
} // namespace IrSlotUi
