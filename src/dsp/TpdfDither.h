#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"
#include <array>
#include <cstdint>
#include <cmath>

/** TPDF dither + quantize. No-op unless 8 <= integerBits < 32. */
class TpdfDither
{
public:
    void prepare (int numChannels) noexcept
    {
        nCh = juce::jlimit (1, Config::kMaxChannels, numChannels);
        reset();
    }

    void reset() noexcept
    {
        rng.fill (0xA341316Cu);
        rng[1] = 0xC816D5A7u;
    }

    void process (juce::dsp::AudioBlock<float>& block, int integerBits) noexcept
    {
        if (integerBits < 8 || integerBits >= 32)
            return;

        const float lsb = std::ldexp (1.0f, 1 - integerBits); // 2^(1-N)
        const float inv = 1.0f / lsb;
        const int chN = juce::jmin (nCh, (int) block.getNumChannels());
        const size_t nS = block.getNumSamples();
        const float clip = 1.0f - lsb;

        for (int ch = 0; ch < chN; ++ch)
        {
            float* d = block.getChannelPointer ((size_t) ch);
            uint32_t s = rng[(size_t) ch];
            for (size_t i = 0; i < nS; ++i)
            {
                float x = d[i];
                if (! std::isfinite (x))
                    x = 0.f;
                const float n = tpdf (s) * lsb;
                float y = (x + n) * inv;
                y = std::nearbyint (y) * lsb;
                d[i] = juce::jlimit (-1.0f, clip, y);
            }
            rng[(size_t) ch] = s;
        }
    }

private:
    static float tpdf (uint32_t& s) noexcept
    {
        s = s * 1664525u + 1013904223u;
        const float a = (float) (s >> 8) * (1.0f / 16777216.0f);
        s = s * 1664525u + 1013904223u;
        const float b = (float) (s >> 8) * (1.0f / 16777216.0f);
        return a - b;
    }

    int nCh { 2 };
    std::array<uint32_t, Config::kMaxChannels> rng {};
};
