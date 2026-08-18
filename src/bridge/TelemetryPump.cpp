#include "TelemetryPump.h"
#include <cmath>
#include <cstring>

namespace bridge
{

void TelemetryPump::reset() noexcept
{
    published.store (0, std::memory_order_relaxed);
    slots[0].size = 0;
    slots[1].size = 0;
}

void TelemetryPump::decimate (const juce::AudioBuffer<float>& src, float* dest, int destN) noexcept
{
    const int n = src.getNumSamples();
    const int ch = src.getNumChannels();
    if (n <= 0 || destN <= 0)
    {
        for (int i = 0; i < destN; ++i)
            dest[i] = 0.f;
        return;
    }
    for (int i = 0; i < destN; ++i)
    {
        const int idx = (int) (((int64_t) i * (int64_t) n) / (int64_t) destN);
        float acc = 0.f;
        const int use = juce::jmax (1, ch);
        for (int c = 0; c < use; ++c)
            acc += src.getReadPointer (c)[juce::jmin (idx, n - 1)];
        dest[i] = acc / (float) use;
    }
}

float TelemetryPump::peakOf (const juce::AudioBuffer<float>& src) noexcept
{
    const int n = src.getNumSamples();
    if (n <= 0)
        return 0.f;
    float p = 0.f;
    for (int c = 0; c < src.getNumChannels(); ++c)
    {
        const float* s = src.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (s[i]);
            if (a > p)
                p = a;
        }
    }
    return p;
}

float TelemetryPump::rmsOf (const juce::AudioBuffer<float>& src) noexcept
{
    const int n = src.getNumSamples();
    if (n <= 0)
        return 0.f;
    double acc = 0.0;
    int count = 0;
    for (int c = 0; c < src.getNumChannels(); ++c)
    {
        const float* s = src.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            acc += (double) s[i] * (double) s[i];
            ++count;
        }
    }
    if (count <= 0)
        return 0.f;
    return (float) std::sqrt (acc / (double) count);
}

void TelemetryPump::noteInput (const juce::AudioBuffer<float>& in) noexcept
{
    decimate (in, scopeIn, kScopeN);
    lastInPeak = peakOf (in);
    lastInRms = rmsOf (in);
}

void TelemetryPump::publish (const juce::AudioBuffer<float>& out, float cpu01) noexcept
{
    decimate (out, scopeOut, kScopeN);
    const int n = out.getNumSamples();
    const float* L = out.getNumChannels() > 0 ? out.getReadPointer (0) : nullptr;
    const float* R = out.getNumChannels() > 1 ? out.getReadPointer (1) : L;
    for (int i = 0; i < kGonioN; ++i)
    {
        const int idx = (n > 0) ? (int) (((int64_t) i * (int64_t) n) / (int64_t) kGonioN) : 0;
        const int ii = (n > 0) ? juce::jmin (idx, n - 1) : 0;
        gonioX[i] = L != nullptr ? L[ii] : 0.f;
        gonioY[i] = R != nullptr ? R[ii] : 0.f;
    }

    TelemetryDesc d;
    d.inPeak = lastInPeak;
    d.outPeak = peakOf (out);
    d.inRms = lastInRms;
    d.outRms = rmsOf (out);
    d.cpu01 = cpu01;
    d.scopeN = (std::uint16_t) kScopeN;
    d.gonioN = (std::uint16_t) kGonioN;

    const int w = 1 - published.load (std::memory_order_relaxed);
    const auto wrote = writeTelemetryFrame (slots[w].bytes, kMaxBytes, d,
                                            scopeIn, scopeOut, gonioX, gonioY);
    slots[w].size = (std::uint32_t) wrote;
    published.store (w, std::memory_order_release);
}

std::size_t TelemetryPump::copyLatest (void* dest, std::size_t destBytes) const noexcept
{
    if (dest == nullptr || destBytes == 0)
        return 0;
    const int r = published.load (std::memory_order_acquire);
    const auto n = (std::size_t) slots[r].size;
    if (n == 0 || n > destBytes)
        return 0;
    std::memcpy (dest, slots[r].bytes, n);
    return n;
}

} // namespace bridge
