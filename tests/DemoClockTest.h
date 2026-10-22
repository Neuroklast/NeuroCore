#pragma once

#include <JuceHeader.h>
#include "../src/core/DemoClock.h"

class DemoClockTest : public juce::UnitTest
{
public:
    DemoClockTest() : juce::UnitTest ("DemoClock", "Licensing") {}

    void runTest() override
    {
        const double t0 = 1.0e12;

        beginTest ("parses the start and the latest seen timestamp");
        {
            auto stamp = DemoClock::parse (DemoClock::serialize (t0, t0 + 5000.0), 0.0);
            expect (stamp.valid);
            expectWithinAbsoluteError (stamp.startedMs, t0, 1.0);
            expectWithinAbsoluteError (stamp.lastSeenMs, t0 + 5000.0, 1.0);
        }

        beginTest ("a single-line legacy stamp is still accepted");
        {
            auto stamp = DemoClock::parse (juce::String ((juce::int64) t0), 0.0);
            expect (stamp.valid);
            expectWithinAbsoluteError (stamp.startedMs, t0, 1.0);
            expectWithinAbsoluteError (stamp.lastSeenMs, t0, 1.0);
        }

        beginTest ("a corrupt stamp falls back to the file age, not a fresh trial");
        {
            auto stamp = DemoClock::parse ("not a timestamp", t0);
            expect (stamp.valid);
            expectEquals (stamp.startedMs, t0);
            expectEquals (stamp.lastSeenMs, t0);

            auto future = DemoClock::parse ("9999999999999999999", 0.0);
            expect (! future.valid);
            auto zero = DemoClock::parse ("0", 0.0);
            expect (! zero.valid);
            auto empty = DemoClock::parse ("", 0.0);
            expect (! empty.valid);
        }

        beginTest ("the demo clock never moves backwards");
        {
            expectEquals (DemoClock::effectiveNow (t0, t0), t0);
            expectEquals (DemoClock::effectiveNow (t0 - 100000.0, t0), t0);
            expectEquals (DemoClock::effectiveNow (t0 + 1000.0, t0), t0 + 1000.0);
        }

        beginTest ("remaining time is capped and clamped");
        {
            const double duration = 14.0 * 86400.0;
            expectEquals (DemoClock::secondsLeft (t0, t0, duration), (int) duration);
            expectEquals (DemoClock::secondsLeft (t0, t0 + 86400.0 * 1000.0, duration),
                          (int) (duration - 86400.0));
            expectEquals (DemoClock::secondsLeft (t0, t0 + duration * 1000.0, duration), 0);
            expectEquals (DemoClock::secondsLeft (t0, t0 + (duration + 10.0) * 1000.0, duration), 0);
            expectEquals (DemoClock::secondsLeft (0.0, t0, duration), 0);
        }

        beginTest ("serialisation round-trips");
        {
            const auto text = DemoClock::serialize (t0, t0 + 1000.0);
            auto stamp = DemoClock::parse (text, 0.0);
            expect (stamp.valid);
            expectWithinAbsoluteError (stamp.startedMs, t0, 1.0);
            expectWithinAbsoluteError (stamp.lastSeenMs, t0 + 1000.0, 1.0);
            expect (DemoClock::serialize (t0 + 10.0, t0) == DemoClock::serialize (t0 + 10.0, t0 + 10.0));
        }
    }
};

