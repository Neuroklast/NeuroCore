#include "TruePeakLimiter.h"
#include <cmath>

float TruePeakLimiter::catmull (float p0, float p1, float p2, float p3, float t) noexcept
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.f * p1)
        + (-p0 + p2) * t
        + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2
        + (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
}

void TruePeakLimiter::prepare (const juce::dsp::ProcessSpec& hostSpec, int osFactor) noexcept
{
    juce::ignoreUnused (osFactor);
    const double sr = hostSpec.sampleRate > 1.0 ? hostSpec.sampleRate : Config::kDefaultSampleRate;
    nCh = juce::jlimit (1, Config::kMaxChannels, (int) hostSpec.numChannels);
    latency = Config::kSanitationLookaheadHost;
    delayLen = latency + 8;
    delay.setSize (nCh, delayLen, false, true, true);
    ceilLin = juce::Decibels::decibelsToGain (Config::kSanitationCeilingDbTp);
    relC = 1.f - std::exp (-1.f / (0.08f * (float) sr));
    reset();
}

void TruePeakLimiter::reset() noexcept
{
    delay.clear();
    writePos = 0;
    gr = 1.f;
}

void TruePeakLimiter::process (juce::dsp::AudioBlock<float>& hostBlock) noexcept
{
    const int chN = juce::jmin (nCh, (int) hostBlock.getNumChannels());
    const int nS = (int) hostBlock.getNumSamples();
    if (chN <= 0 || nS <= 0 || delayLen <= 0)
        return;

    const int tpN = Config::kSanitationTpFactor;
    for (int i = 0; i < nS; ++i)
    {
        for (int ch = 0; ch < chN; ++ch)
            delay.setSample (ch, writePos, hostBlock.getChannelPointer ((size_t) ch)[i]);

        const int outPos = (writePos - latency + delayLen) % delayLen;
        float tp = 0.f;
        for (int ch = 0; ch < chN; ++ch)
        {
            const float p0 = delay.getSample (ch, (outPos - 1 + delayLen) % delayLen);
            const float p1 = delay.getSample (ch, outPos);
            const float p2 = delay.getSample (ch, (outPos + 1) % delayLen);
            const float p3 = delay.getSample (ch, (outPos + 2) % delayLen);
            tp = juce::jmax (tp, std::abs (p1), std::abs (p2));
            for (int k = 1; k < tpN; ++k)
            {
                const float t = (float) k / (float) tpN;
                tp = juce::jmax (tp, std::abs (catmull (p0, p1, p2, p3, t)));
            }
        }
        if (! std::isfinite (tp))
            tp = 0.f;

        const float needed = (tp > ceilLin && tp > 1.0e-12f) ? (ceilLin / tp) : 1.f;
        if (needed < gr)
            gr = needed;
        else
            gr += relC * (1.f - gr);
        if (! std::isfinite (gr))
            gr = needed;
        gr = juce::jlimit (0.f, 1.f, gr);
        if (gr < 0.999f)
            hit.store (true, std::memory_order_relaxed);

        for (int ch = 0; ch < chN; ++ch)
        {
            float y = delay.getSample (ch, outPos) * gr;
            if (! std::isfinite (y))
                y = 0.f;
            else if (std::abs (y) > ceilLin)
                y = std::copysign (ceilLin, y);
            hostBlock.getChannelPointer ((size_t) ch)[i] = y;
        }

        writePos = (writePos + 1) % delayLen;
    }
}
