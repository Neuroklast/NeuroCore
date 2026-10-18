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
    std::fill (std::begin (scopeIn), std::end (scopeIn), 0.f);
    std::fill (std::begin (scopeOut), std::end (scopeOut), 0.f);
    lastInPeak = lastInRms = 0.f;
    envFast = envSlow = 0.f;
    onsetHold = 0;
    onsetCount = 0;
    onsetAmp = 0.f;
    onsetWrite = 0;
    for (auto& e : onsetRing)
        e = {};
    seq = 0;
    samplePos = 0;
}

void TelemetryPump::captureScope (const juce::AudioBuffer<float>& src, float* dest, int destN) noexcept
{
    const int n = src.getNumSamples();
    const int ch = src.getNumChannels();
    if (destN <= 0 || n <= 0) return;
    if (ch <= 0) { std::fill (dest, dest + destN, 0.f); return; }
    // Scope/FFT timestamps are at the host rate: never stretch a short block
    // or decimate a long block while labelling it with the original rate.
    const int take = juce::jmin (n, destN);
    std::move (dest + take, dest + destN, dest);
    for (int i = 0; i < take; ++i)
    {
        float sum = 0.f;
        for (int c = 0; c < ch; ++c)
            sum += src.getReadPointer (c)[n - take + i];
        dest[destN - take + i] = sum / (float) ch;
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

void TelemetryPump::scanOnset (const juce::AudioBuffer<float>& src) noexcept
{
    const int n = src.getNumSamples();
    const int ch = src.getNumChannels();
    if (n <= 0 || ch <= 0)
        return;
    const float sr = hostSr > 1.0 ? (float) hostSr : 48000.f;
    const float dt = 1.f / sr;
    const float fr = 1.f - std::exp (-dt / 0.05f);
    const float sa = 1.f - std::exp (-dt / 0.025f);
    const float sl = 1.f - std::exp (-dt / 0.22f);
    const int holdN = juce::jmax (1, (int) std::round (0.045 * (double) sr));
    for (int i = 0; i < n; ++i)
    {
        float a = 0.f;
        for (int c = 0; c < ch; ++c)
        {
            const float s = std::abs (src.getReadPointer (c)[i]);
            if (s > a)
                a = s;
        }
        if (a > envFast)
            envFast = a;
        else
            envFast += (a - envFast) * fr;
        envSlow += (a - envSlow) * (a > envSlow ? sa : sl);
        if (onsetHold > 0)
        {
            --onsetHold;
            continue;
        }
        const float flux = envFast - envSlow;
        const float gate = 0.055f + 0.42f * envSlow;
        if (flux > gate && a > 0.12f)
        {
            float ev = flux > 1.f ? 1.f : flux;
            if (onsetCount < 255)
                ++onsetCount;
            if (ev > onsetAmp)
                onsetAmp = ev;
            const int slot = (int) (onsetWrite % (std::uint32_t) kTelemetryMaxOnsets);
            onsetRing[slot].samplePos = samplePos + (std::uint32_t) i;
            onsetRing[slot].amp = ev;
            ++onsetWrite;
            onsetHold = holdN;
        }
    }
}

void TelemetryPump::noteInput (const juce::AudioBuffer<float>& in) noexcept
{
    if (! wanted.load (std::memory_order_relaxed))
        return;
    captureScope (in, scopeIn, kScopeN);
    lastInPeak = peakOf (in);
    lastInRms = rmsOf (in);
}

void TelemetryPump::publish (const juce::AudioBuffer<float>& out, float cpu01) noexcept
{
    if (! wanted.load (std::memory_order_relaxed))
        return;
    captureScope (out, scopeOut, kScopeN);
    const int n = out.getNumSamples();
    const float* L = out.getNumChannels() > 0 ? out.getReadPointer (0) : nullptr;
    const float* R = out.getNumChannels() > 1 ? out.getReadPointer (1) : L;
    for (int i = 0; i < kGonioN; ++i)
    {
        const int idx = (n > 0) ? (int) (((int64_t) i * (int64_t) n) / (int64_t) kGonioN) : 0;
        const int ii = (n > 0) ? juce::jmin (idx, n - 1) : 0;
        gonioX[i] = n > 0 && L != nullptr ? L[ii] : 0.f;
        gonioY[i] = n > 0 && R != nullptr ? R[ii] : 0.f;
    }

    TelemetryDesc d;
    d.inPeak = lastInPeak;
    d.outPeak = peakOf (out);
    d.inRms = lastInRms;
    d.outRms = rmsOf (out);
    scanOnset (out);
    d.cpu01 = cpu01;
    d.scopeN = (std::uint16_t) kScopeN;
    d.gonioN = (std::uint16_t) kGonioN;
    d.onsetCount = onsetCount;
    d.onsetAmp = onsetAmp > 1.f ? 1.f : onsetAmp;
    {
        const std::uint32_t total = onsetWrite;
        const int nOn = (int) juce::jmin ((std::uint32_t) kTelemetryMaxOnsets, total);
        d.onsetN = (std::uint8_t) nOn;
        const std::uint32_t start = total - (std::uint32_t) nOn;
        for (int i = 0; i < nOn; ++i)
            d.onsets[i] = onsetRing[(start + (std::uint32_t) i) % (std::uint32_t) kTelemetryMaxOnsets];
    }
    onsetCount = 0;
    onsetAmp = 0.f;
    seq += 1;
    d.seq = seq;
    d.samplePos = samplePos;
    samplePos += (std::uint32_t) juce::jmax (0, n);

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
