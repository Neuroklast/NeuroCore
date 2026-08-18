#pragma once

#include <JuceHeader.h>
#include "../src/bridge/TelemetryPump.h"

class TelemetryPumpTest : public juce::UnitTest
{
public:
    TelemetryPumpTest() : juce::UnitTest ("TelemetryPump", "Bridge") {}

    void runTest() override
    {
        beginTest ("publish then copyLatest has NKTM and peaks");
        {
            bridge::TelemetryPump pump;
            juce::AudioBuffer<float> in (2, 64), out (2, 64);
            for (int i = 0; i < 64; ++i)
            {
                in.setSample (0, i, 0.5f);
                in.setSample (1, i, 0.5f);
                out.setSample (0, i, 0.25f);
                out.setSample (1, i, -0.25f);
            }
            pump.noteInput (in);
            pump.publish (out, 0.42f);

            std::uint8_t buf[4096];
            const auto n = pump.copyLatest (buf, sizeof (buf));
            expect (n >= bridge::kTelemetryHeaderBytes);

            bridge::TelemetryDesc d;
            expect (bridge::readTelemetryHeader (buf, n, d));
            expectEquals ((int) d.scopeN, bridge::TelemetryPump::kScopeN);
            expectEquals ((int) d.gonioN, bridge::TelemetryPump::kGonioN);
            expect (d.inPeak > 0.4f);
            expect (d.outPeak > 0.2f);
            expect (std::abs (d.cpu01 - 0.42f) < 1.0e-5f);

            const auto* words = reinterpret_cast<const std::uint32_t*> (buf);
            expectEquals ((int) words[0], (int) bridge::kTelemetryMagic);
        }

        beginTest ("empty pump copies nothing");
        {
            bridge::TelemetryPump pump;
            std::uint8_t buf[64];
            expectEquals ((int) pump.copyLatest (buf, sizeof (buf)), 0);
        }
    }
};
