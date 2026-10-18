#pragma once

#include <cstddef>
#include <cstdint>

namespace bridge
{

inline constexpr std::uint32_t kTelemetryMagic = 0x4E4B544D; // 'NKTM'
inline constexpr std::uint16_t kTelemetryVersion = 3;
inline constexpr std::uint16_t kTelemetryVersionMin = 1;
inline constexpr std::size_t kTelemetryHeaderBytes = 32;
inline constexpr std::size_t kTelemetryTailBytes = 8;
inline constexpr int kTelemetryMaxOnsets = 16;
inline constexpr std::size_t kTelemetryOnsetBytes = 12;
inline constexpr std::size_t kTelemetryOnsetListHeader = 4;

struct OnsetEvent
{
    std::uint32_t samplePos { 0 };
    float amp { 0.f };
    float balance { 0.f };
};

struct TelemetryDesc
{
    float inPeak { 0.f };
    float outPeak { 0.f };
    float inRms { 0.f };
    float outRms { 0.f };
    float cpu01 { 0.f };
    std::uint16_t scopeN { 0 };
    std::uint16_t gonioN { 0 };
    std::uint8_t onsetCount { 0 };
    float onsetAmp { 0.f };
    std::uint32_t seq { 0 };
    std::uint32_t samplePos { 0 };
    std::uint8_t onsetN { 0 };
    OnsetEvent onsets[kTelemetryMaxOnsets] {};
};

inline std::size_t telemetryByteSize (const TelemetryDesc& d) noexcept
{
    return kTelemetryHeaderBytes
         + (std::size_t) d.scopeN * sizeof (float) * 2
         + (std::size_t) d.gonioN * sizeof (float) * 2
         + kTelemetryTailBytes
         + kTelemetryOnsetListHeader
         + (std::size_t) d.onsetN * kTelemetryOnsetBytes;
}

/** Little-endian frame. Returns bytes written, or 0 if dest is too small. */
std::size_t writeTelemetryFrame (void* dest, std::size_t destBytes, const TelemetryDesc& desc,
                                 const float* scopeIn, const float* scopeOut,
                                 const float* gonioX, const float* gonioY) noexcept;

bool readTelemetryHeader (const void* src, std::size_t srcBytes, TelemetryDesc& dest) noexcept;

} // namespace bridge
