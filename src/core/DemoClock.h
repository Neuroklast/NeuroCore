#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
 * Persistent 14-day demo clock.
 *
 * The stamp file stores two lines: the first launch timestamp and the latest
 * observed timestamp. Using the latest observed value means that moving the
 * system clock backwards cannot extend the demo. A corrupt or missing file
 * falls back to the file's modification time, so deleting the content does not
 * reset the trial either.
 */
namespace DemoClock
{
struct Stamp
{
    double startedMs = 0.0;
    double lastSeenMs = 0.0;
    bool valid = false;
};

inline Stamp parse (const juce::String& text, double fileModifiedMs) noexcept
{
    Stamp stamp;
    auto lines = juce::StringArray::fromLines (text.trim());
    const auto started = lines.isEmpty() ? 0.0 : lines[0].trim().getDoubleValue();
    if (started > 0.0 && started <= 1.0e15)
    {
        stamp.startedMs = started;
        stamp.lastSeenMs = started;
        if (lines.size() > 1)
        {
            const auto seen = lines[1].trim().getDoubleValue();
            if (seen > stamp.lastSeenMs && seen <= 1.0e15)
                stamp.lastSeenMs = seen;
        }
        stamp.valid = true;
        return stamp;
    }

    // Corrupt content must not restart the trial: use the file age instead.
    if (fileModifiedMs > 0.0 && fileModifiedMs <= 1.0e15)
    {
        stamp.startedMs = fileModifiedMs;
        stamp.lastSeenMs = fileModifiedMs;
        stamp.valid = true;
    }
    return stamp;
}

inline juce::String serialize (double startedMs, double lastSeenMs)
{
    // Integer milliseconds: exact round-trip without locale or scientific notation.
    const auto started = (juce::int64) std::llround (startedMs);
    const auto seen = (juce::int64) std::llround (juce::jmax (startedMs, lastSeenMs));
    return juce::String (started) + "\n" + juce::String (seen);
}

/** Wall clock that can only move forward for demo purposes. */
inline double effectiveNow (double nowMs, double lastSeenMs) noexcept
{
    return juce::jmax (nowMs, lastSeenMs);
}

inline int secondsLeft (double startedMs, double effectiveNowMs, double durationSeconds) noexcept
{
    if (startedMs <= 0.0)
        return 0;
    const double elapsed = (effectiveNowMs - startedMs) / 1000.0;
    if (elapsed <= 0.0)
        return (int) juce::jmax (0.0, std::ceil (durationSeconds));
    const double remaining = durationSeconds - elapsed;
    if (! (remaining > 0.0))
        return 0;
    return (int) std::ceil (juce::jmin (remaining, durationSeconds));
}
} // namespace DemoClock
