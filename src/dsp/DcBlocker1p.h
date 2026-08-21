#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"
#include <algorithm>
#include <cmath>

/** First-order DC blocker. y[n] = R * (y[n-1] + x[n] - x[n-1]). */
class DcBlocker1p
{
public:
    void prepare (double sampleRate, int numChannels) noexcept
    {
        const double fs = sampleRate > 1.0 ? sampleRate : Config::kDefaultSampleRate;
        const double fc = (double) Config::kSanitationDcHz;
        double r = 1.0 - (2.0 * juce::MathConstants<double>::pi * fc / fs);
        R = (float) juce::jlimit (0.9, 0.999999, r);
        nCh = juce::jlimit (1, Config::kMaxChannels, numChannels);
        reset();
    }

    void reset() noexcept
    {
        std::fill (x1, x1 + Config::kMaxChannels, 0.f);
        std::fill (y1, y1 + Config::kMaxChannels, 0.f);
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int chN = juce::jmin (nCh, (int) block.getNumChannels());
        const size_t nS = block.getNumSamples();
        for (int ch = 0; ch < chN; ++ch)
        {
            float* NK_RESTRICT d = block.getChannelPointer ((size_t) ch);
            float xPrev = x1[(size_t) ch];
            float yPrev = y1[(size_t) ch];
            for (size_t i = 0; i < nS; ++i)
            {
                const float x = d[i];
                float y = R * (yPrev + x - xPrev);
                if (! std::isfinite (y))
                    y = 0.f;
                d[i] = y;
                xPrev = x;
                yPrev = y;
            }
            x1[(size_t) ch] = xPrev;
            y1[(size_t) ch] = yPrev;
        }
    }

private:
    float R { 0.999f };
    int nCh { 2 };
    alignas (64) float x1[Config::kMaxChannels] {};
    alignas (64) float y1[Config::kMaxChannels] {};
};
