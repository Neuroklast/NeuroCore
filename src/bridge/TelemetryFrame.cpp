#include "TelemetryFrame.h"

#include <cstring>

namespace bridge
{
namespace
{

void writeU16 (std::uint8_t* p, std::uint16_t v) noexcept
{
    p[0] = (std::uint8_t) (v & 0xff);
    p[1] = (std::uint8_t) ((v >> 8) & 0xff);
}

void writeU32 (std::uint8_t* p, std::uint32_t v) noexcept
{
    p[0] = (std::uint8_t) (v & 0xff);
    p[1] = (std::uint8_t) ((v >> 8) & 0xff);
    p[2] = (std::uint8_t) ((v >> 16) & 0xff);
    p[3] = (std::uint8_t) ((v >> 24) & 0xff);
}

void writeF32 (std::uint8_t* p, float v) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &v, sizeof (bits));
    writeU32 (p, bits);
}

std::uint16_t readU16 (const std::uint8_t* p) noexcept
{
    return (std::uint16_t) p[0] | (std::uint16_t) ((std::uint16_t) p[1] << 8);
}

std::uint32_t readU32 (const std::uint8_t* p) noexcept
{
    return (std::uint32_t) p[0]
         | ((std::uint32_t) p[1] << 8)
         | ((std::uint32_t) p[2] << 16)
         | ((std::uint32_t) p[3] << 24);
}

float readF32 (const std::uint8_t* p) noexcept
{
    const std::uint32_t bits = readU32 (p);
    float v = 0.f;
    std::memcpy (&v, &bits, sizeof (v));
    return v;
}

void copyFloats (std::uint8_t*& p, const float* src, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        writeF32 (p, src != nullptr ? src[i] : 0.f);
        p += 4;
    }
}

} // namespace

std::size_t writeTelemetryFrame (void* dest, std::size_t destBytes, const TelemetryDesc& desc,
                                 const float* scopeIn, const float* scopeOut,
                                 const float* gonioX, const float* gonioY) noexcept
{
    const auto need = telemetryByteSize (desc);
    if (dest == nullptr || destBytes < need)
        return 0;

    auto* p = static_cast<std::uint8_t*> (dest);
    writeU32 (p + 0, kTelemetryMagic);
    writeU16 (p + 4, kTelemetryVersion);
    writeU16 (p + 6, 0);
    writeF32 (p + 8, desc.inPeak);
    writeF32 (p + 12, desc.outPeak);
    writeF32 (p + 16, desc.inRms);
    writeF32 (p + 20, desc.outRms);
    writeF32 (p + 24, desc.cpu01);
    writeU16 (p + 28, desc.scopeN);
    writeU16 (p + 30, desc.gonioN);

    auto* body = p + kTelemetryHeaderBytes;
    copyFloats (body, scopeIn, desc.scopeN);
    copyFloats (body, scopeOut, desc.scopeN);
    copyFloats (body, gonioX, desc.gonioN);
    copyFloats (body, gonioY, desc.gonioN);
    return need;
}

bool readTelemetryHeader (const void* src, std::size_t srcBytes, TelemetryDesc& dest) noexcept
{
    if (src == nullptr || srcBytes < kTelemetryHeaderBytes)
        return false;

    const auto* p = static_cast<const std::uint8_t*> (src);
    if (readU32 (p) != kTelemetryMagic)
        return false;
    if (readU16 (p + 4) != kTelemetryVersion)
        return false;

    dest.inPeak = readF32 (p + 8);
    dest.outPeak = readF32 (p + 12);
    dest.inRms = readF32 (p + 16);
    dest.outRms = readF32 (p + 20);
    dest.cpu01 = readF32 (p + 24);
    dest.scopeN = readU16 (p + 28);
    dest.gonioN = readU16 (p + 30);
    return telemetryByteSize (dest) <= srcBytes;
}

} // namespace bridge
