#pragma once

#include <cstddef>
#include <cstdint>

namespace bridge
{

inline constexpr std::uint32_t kTelemetryMagic = 0x4E4B544D; // 'NKTM'
inline constexpr std::uint16_t kTelemetryVersion = 1;
inline constexpr std::size_t kTelemetryHeaderBytes = 32;

struct TelemetryDesc
{
    float inPeak { 0.f };
    float outPeak { 0.f };
    float inRms { 0.f };
    float outRms { 0.f };
    float cpu01 { 0.f };
    std::uint16_t scopeN { 0 };
    std::uint16_t gonioN { 0 };
};

inline std::size_t telemetryByteSize (const TelemetryDesc& d) noexcept
{
    return kTelemetryHeaderBytes
         + (std::size_t) d.scopeN * sizeof (float) * 2
         + (std::size_t) d.gonioN * sizeof (float) * 2;
}

/** Little-endian frame. Returns bytes written, or 0 if dest is too small. */
std::size_t writeTelemetryFrame (void* dest, std::size_t destBytes, const TelemetryDesc& desc,
                                 const float* scopeIn, const float* scopeOut,
                                 const float* gonioX, const float* gonioY) noexcept;

bool readTelemetryHeader (const void* src, std::size_t srcBytes, TelemetryDesc& dest) noexcept;

} // namespace bridge
