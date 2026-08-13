#pragma once

#include <JuceHeader.h>
#include <cmath>

/** Website-style modal window assemble (clip-path), not a scanline loop. */
enum class ClipReveal : uint8_t
{
    CircuitBreak,   // vertical slit from center -> full width
    SystemBoot,     // top -> bottom
    GlitchScan,     // left -> right
    DataStream,     // bottom -> top
    MatrixDecode,   // center out
    RingLink        // horizontal band from mid -> full height
};

inline constexpr float kClipRevealSec = 0.34f;

inline ClipReveal randomClipReveal (juce::Random& rng) noexcept
{
    return (ClipReveal) rng.nextInt (6);
}

inline float clipEase (float t) noexcept
{
    t = juce::jlimit (0.f, 1.f, t);
    return t * t * (3.f - 2.f * t);
}

/** t=0 empty-ish, t=1 == full. Matches neuroklast overlay-animations.ts */
inline juce::Rectangle<int> revealBounds (juce::Rectangle<int> full, ClipReveal type, float t) noexcept
{
    t = clipEase (t);
    if (t >= 0.999f)
        return full;
    if (full.isEmpty())
        return full;

    const int w = full.getWidth();
    const int h = full.getHeight();

    switch (type)
    {
        case ClipReveal::SystemBoot:
        {
            const int nh = juce::jmax (2, (int) std::round ((float) h * t));
            return { full.getX(), full.getY(), w, nh };
        }
        case ClipReveal::GlitchScan:
        {
            const int nw = juce::jmax (2, (int) std::round ((float) w * t));
            return { full.getX(), full.getY(), nw, h };
        }
        case ClipReveal::DataStream:
        {
            const int nh = juce::jmax (2, (int) std::round ((float) h * t));
            return { full.getX(), full.getBottom() - nh, w, nh };
        }
        case ClipReveal::CircuitBreak:
        {
            const int nw = juce::jmax (2, (int) std::round ((float) w * t));
            return full.withSizeKeepingCentre (nw, h);
        }
        case ClipReveal::RingLink:
        {
            const int nh = juce::jmax (2, (int) std::round ((float) h * t));
            return full.withSizeKeepingCentre (w, nh);
        }
        case ClipReveal::MatrixDecode:
        default:
        {
            const int nw = juce::jmax (2, (int) std::round ((float) w * t));
            const int nh = juce::jmax (2, (int) std::round ((float) h * t));
            return full.withSizeKeepingCentre (nw, nh);
        }
    }
}
