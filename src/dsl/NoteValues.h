#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <vector>
#include "../core/Config.h"

/** Musical note lengths for `param a = Time [1/1, 1/16]` (whole-note fractions). */
namespace dsl
{
namespace NoteValues
{
    struct Step
    {
        float whole;          ///< 1/4 → 0.25 of a whole note
        const char* label;
    };

    /// Longest → shortest. Dotted and triplet in-betweens included.
    inline constexpr Step kGrid[] = {
        { 1.0f,      "1/1" },
        { 0.75f,     "1/2." },
        { 0.5f,      "1/2" },
        { 0.375f,    "1/4." },
        { 1.0f / 3.0f, "1/3" },
        { 0.25f,     "1/4" },
        { 0.1875f,   "1/8." },
        { 1.0f / 6.0f, "1/6" },
        { 0.125f,    "1/8" },
        { 0.09375f,  "1/16." },
        { 1.0f / 12.0f, "1/12" },
        { 0.0625f,   "1/16" },
    };

    inline constexpr float kEps = 1.0e-4f;

    inline bool parseToken (juce::String s, float& wholeOut) noexcept
    {
        s = s.trim().toLowerCase();
        if (s.isEmpty())
            return false;

        bool dotted = false;
        bool triplet = false;
        if (s.endsWithChar ('.') || s.endsWithChar ('d'))
        {
            dotted = true;
            s = s.dropLastCharacters (1).trim();
        }
        else if (s.endsWithChar ('t'))
        {
            triplet = true;
            s = s.dropLastCharacters (1).trim();
        }

        float whole = 0.f;
        if (s == "bar" || s == "1bar")
        {
            whole = 1.f;
        }
        else
        {
            const int slash = s.indexOfChar ('/');
            if (slash <= 0)
                return false;
            const float num = s.substring (0, slash).trim().getFloatValue();
            const float den = s.substring (slash + 1).trim().getFloatValue();
            if (den == 0.f || ! std::isfinite (num) || ! std::isfinite (den))
                return false;
            whole = num / den;
        }

        if (dotted)
            whole *= 1.5f;
        if (triplet)
            whole *= (2.0f / 3.0f);
        if (! (whole > 0.f) || ! std::isfinite (whole))
            return false;
        wholeOut = whole;
        return true;
    }

    inline juce::String labelFor (float whole) noexcept
    {
        for (const auto& g : kGrid)
            if (std::abs (g.whole - whole) < kEps)
                return g.label;
        const int den = juce::jlimit (1, 64, (int) std::lround (1.0 / (double) whole));
        return "1/" + juce::String (den);
    }

    struct Grid
    {
        std::vector<float> wholes;
        std::vector<juce::String> labels;

        int size() const noexcept { return (int) wholes.size(); }

        int indexFromNorm (float norm) const noexcept
        {
            const int n = size();
            if (n <= 1)
                return 0;
            return juce::jlimit (0, n - 1, (int) std::lround (norm * (float) (n - 1)));
        }

        float wholeFromNorm (float norm) const noexcept
        {
            if (wholes.empty())
                return 0.25f;
            return wholes[(size_t) indexFromNorm (norm)];
        }

        juce::String labelFromNorm (float norm) const noexcept
        {
            if (labels.empty())
                return {};
            return labels[(size_t) indexFromNorm (norm)];
        }

        float msFromNorm (float norm, double bpm) const noexcept
        {
            const double b = bpm > 1.0 ? bpm : Config::kDefaultTempo;
            const float beats = wholeFromNorm (norm) * 4.f;
            return (float) ((60000.0 / b) * (double) beats);
        }
    };

    inline bool looksLikeNote (const juce::String& token) noexcept
    {
        float w = 0.f;
        return parseToken (token, w);
    }

    inline Grid makeGrid (float wholeA, float wholeB) noexcept
    {
        Grid g;
        const float lo = juce::jmin (wholeA, wholeB);
        const float hi = juce::jmax (wholeA, wholeB);
        for (const auto& step : kGrid)
            if (step.whole + kEps >= lo && step.whole - kEps <= hi)
            {
                g.wholes.push_back (step.whole);
                g.labels.push_back (step.label);
            }

        if (g.wholes.empty())
        {
            g.wholes.push_back (wholeA);
            g.wholes.push_back (wholeB);
            g.labels.push_back (labelFor (wholeA));
            g.labels.push_back (labelFor (wholeB));
        }

        // Grid is stored longest→shortest. Flip if the user wrote short→long.
        if (wholeA + kEps < wholeB)
        {
            std::reverse (g.wholes.begin(), g.wholes.end());
            std::reverse (g.labels.begin(), g.labels.end());
        }
        return g;
    }
}
}
