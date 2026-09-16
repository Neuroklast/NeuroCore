#pragma once
#include <JuceHeader.h>
#include "../src/bridge/TelemetryPump.h"
#include <cstring>

class TelemetrySamplingTest : public juce::UnitTest
{
public:
    TelemetrySamplingTest() : UnitTest ("TelemetrySampling", "Bridge") {}
    void runTest() override
    {
        beginTest ("scope is a contiguous host-rate window regardless of callback size");
        for (int blockSize : { 1, 64, 256, 512 })
        {
            bridge::TelemetryPump pump;
            juce::AudioBuffer<float> b (2, blockSize);
            const int blocks = 512 / blockSize;
            for (int block = 0; block < blocks; ++block)
            {
                for (int i = 0; i < blockSize; ++i)
                    for (int c = 0; c < 2; ++c)
                        b.setSample (c, i, 0.2f * std::sin ((block * blockSize + i) * 0.057f));
                pump.noteInput (b);
                pump.publish (b, 0.f);
            }
            std::uint8_t frame[bridge::TelemetryPump::kMaxBytes] {};
            expect (pump.copyLatest (frame, sizeof (frame)) > 32);
            float error = 0.f;
            for (int i = 0; i < bridge::TelemetryPump::kScopeN; ++i)
            {
                float actual = 0;
                std::memcpy (&actual, frame + 32 + i * sizeof (float), sizeof (float));
                error = std::max (error, std::abs (actual - 0.2f * std::sin ((256 + i) * 0.057f)));
            }
            expect (error < 1.e-6f, "block=" + juce::String (blockSize) + " resampling error=" + juce::String (error));
        }
    }
};
