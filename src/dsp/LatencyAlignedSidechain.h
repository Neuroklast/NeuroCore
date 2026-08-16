#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST

    Architecture contract: wet path latency must equal dry sidechain delay.

    Oversampling FIR and DryWetMixer delay the wet path by N samples.
    Any comparison of dry vs wet (AutoGain, residual policy, diagnostics)
    MUST use dry delayed by the same N. Comparing raw dry against delayed wet
    produces sidechain fighting: GR/gate pumps at smp≈N and intensifies over time.

    This class is the single owner of that delay line — not a threshold tweak.
*/

#include <JuceHeader.h>

class LatencyAlignedSidechain
{
public:
    void prepare (int channels, int maxBlockSize, int latencySamples) noexcept
    {
        channels_ = juce::jmax (1, channels);
        latency_  = juce::jmax (0, latencySamples);
        const int ringLen = juce::jmax (maxBlockSize, latency_ + maxBlockSize + 8);
        if (ring_.getNumChannels() != channels_
            || ring_.getNumSamples() < ringLen
            || lastLatency_ != latency_)
        {
            ring_.setSize (channels_, ringLen, false, true, true);
            ring_.clear();
            writePos_ = 0;
            lastLatency_ = latency_;
        }
        if (aligned_.getNumChannels() != channels_
            || aligned_.getNumSamples() < maxBlockSize)
        {
            aligned_.setSize (channels_, maxBlockSize, false, true, true);
            aligned_.clear();
        }
    }

    void reset() noexcept
    {
        ring_.clear();
        aligned_.clear();
        writePos_ = 0;
    }

    int getLatency() const noexcept { return latency_; }

    /** Push current-block dry; fill aligned buffer delayed by prepare() latency. */
    void pushAndRead (const juce::AudioBuffer<float>& dry, int numSamples) noexcept
    {
        const int nCh = juce::jmin (channels_, dry.getNumChannels());
        const int ringN = ring_.getNumSamples();
        if (nCh <= 0 || ringN <= 0 || numSamples <= 0)
            return;

        if (aligned_.getNumSamples() < numSamples
            || aligned_.getNumChannels() != nCh)
            aligned_.setSize (nCh, numSamples, false, false, true);

        if (latency_ == 0)
        {
            for (int ch = 0; ch < nCh; ++ch)
                aligned_.copyFrom (ch, 0, dry, ch, 0, numSamples);
            return;
        }

        // Block-oriented ring update (same modular delay as sample loop).
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* src = dry.getReadPointer (ch);
            float* ring = ring_.getWritePointer (ch);
            float* dst = aligned_.getWritePointer (ch);

            int w = writePos_;
            for (int i = 0; i < numSamples; ++i)
            {
                const int r = (w - latency_ + ringN * 8) % ringN;
                ring[w] = src[i];
                dst[i] = ring[r];
                w = (w + 1) % ringN;
            }
        }
        writePos_ = (writePos_ + numSamples) % ringN;
    }

    const juce::AudioBuffer<float>& getAligned() const noexcept { return aligned_; }
    juce::AudioBuffer<float>&       getAligned()       noexcept { return aligned_; }

    /** Test helper: impulse response delay equals configured latency. */
    static int measureImpulseDelay (int latencySamples, int blockSize = 64) noexcept
    {
        LatencyAlignedSidechain sc;
        sc.prepare (1, blockSize, latencySamples);
        juce::AudioBuffer<float> dry (1, blockSize);
        dry.clear();
        dry.setSample (0, 0, 1.0f);
        sc.pushAndRead (dry, blockSize);
        int total = 0;
        for (int pass = 0; pass < 8; ++pass)
        {
            const auto& a = sc.getAligned();
            for (int i = 0; i < blockSize; ++i)
            {
                if (std::abs (a.getSample (0, i)) > 0.5f)
                    return total + i;
            }
            dry.clear();
            sc.pushAndRead (dry, blockSize);
            total += blockSize;
        }
        return -1;
    }

private:
    juce::AudioBuffer<float> ring_;
    juce::AudioBuffer<float> aligned_;
    int channels_ { 2 };
    int latency_ { 0 };
    int lastLatency_ { -1 };
    int writePos_ { 0 };
};
