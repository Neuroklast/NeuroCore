#pragma once

#include "TelemetryFrame.h"
#include <JuceHeader.h>
#include <atomic>
#include <cstdint>

namespace bridge
{

/** Latest-value SPSC telemetry. Audio thread publishes; UI copies. No heap in push. */
class TelemetryPump
{
public:
    static constexpr int kScopeN = 256;
    static constexpr int kGonioN = 128;
    static constexpr std::size_t kMaxBytes = 4096;

    void reset() noexcept;
    void noteInput (const juce::AudioBuffer<float>& in) noexcept;
    void publish (const juce::AudioBuffer<float>& out, float cpu01) noexcept;
    std::size_t copyLatest (void* dest, std::size_t destBytes) const noexcept;

private:
    struct Slot
    {
        std::uint8_t bytes[kMaxBytes] {};
        std::uint32_t size { 0 };
    };

    void decimate (const juce::AudioBuffer<float>& src, float* dest, int destN) noexcept;
    static float peakOf (const juce::AudioBuffer<float>& src) noexcept;
    static float rmsOf (const juce::AudioBuffer<float>& src) noexcept;

    Slot slots[2];
    std::atomic<int> published { 0 };
    float scopeIn[kScopeN] {};
    float scopeOut[kScopeN] {};
    float gonioX[kGonioN] {};
    float gonioY[kGonioN] {};
    float lastInPeak { 0.f };
    float lastInRms { 0.f };
};

} // namespace bridge
