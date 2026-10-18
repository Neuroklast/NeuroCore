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
    static constexpr int kScopeN = 2048;
    static constexpr int kGonioN = 128;
    static constexpr std::size_t kMaxBytes = 20480;

    void reset() noexcept;
    void setSampleRate (double sr) noexcept { hostSr = sr > 1.0 ? sr : 48000.0; }
    /** Audio thread: skip noteInput/publish when no editor is reading NKTM. */
    void setWanted (bool on) noexcept { wanted.store (on, std::memory_order_relaxed); }
    bool isWanted() const noexcept { return wanted.load (std::memory_order_relaxed); }
    void noteInput (const juce::AudioBuffer<float>& in) noexcept;
    void publish (const juce::AudioBuffer<float>& out, float cpu01) noexcept;
    std::size_t copyLatest (void* dest, std::size_t destBytes) const noexcept;

private:
    struct Slot
    {
        std::uint8_t bytes[kMaxBytes] {};
        std::uint32_t size { 0 };
    };

    void captureScope (const juce::AudioBuffer<float>& src, float* dest, int destN) noexcept;
    static float peakOf (const juce::AudioBuffer<float>& src) noexcept;
    static float rmsOf (const juce::AudioBuffer<float>& src) noexcept;

    Slot slots[2];
    std::atomic<int> published { 0 };
    std::atomic<bool> wanted { true };
    float scopeIn[kScopeN] {};
    float scopeOut[kScopeN] {};
    float gonioX[kGonioN] {};
    float gonioY[kGonioN] {};
    float lastInPeak { 0.f };
    float lastInRms { 0.f };
    double hostSr { 48000.0 };
    std::uint32_t seq { 0 };
    std::uint32_t samplePos { 0 };
    float envFast { 0.f };
    float envSlow { 0.f };
    int onsetHold { 0 };
    std::uint8_t onsetCount { 0 };
    float onsetAmp { 0.f };

    void scanOnset (const juce::AudioBuffer<float>& src) noexcept;
};

} // namespace bridge
