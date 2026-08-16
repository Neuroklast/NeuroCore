#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST

    Sole end-of-chain safety boundary (do not re-implement elsewhere):
      - NaN/Inf hold (never hard-zero)
      - Soft peak limiter (smooth GR near full scale)
      - Residual mute ONLY when dry AND wet are both at denoise floor
      - Denormal flush

    Architecture rules:
      - AutoGain / NoiseGate / stage soft-ceil MUST NOT also mute residual
      - Musical character (drive, sustain, delay tails) is NOT managed here
      - Sidechain dry MUST be latency-aligned (LatencyAlignedSidechain)
      - Crackle → fix timeline/state, do not raise/lower these floors ad-hoc
*/

#include <JuceHeader.h>
#include <array>
#include <cmath>

class OutputSanitizer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        numChannels = juce::jlimit (1, 8, (int) spec.numChannels);

        const double limAtkSec = 0.003;
        const double limRelSec = 0.100;
        limAtk = (float) std::exp (-1.0 / (limAtkSec * sampleRate));
        limRel = (float) std::exp (-1.0 / (limRelSec * sampleRate));

        const double envAtkSec = 0.002;
        const double envRelSec = 0.080;
        envAtk = (float) std::exp (-1.0 / (envAtkSec * sampleRate));
        envRel = (float) std::exp (-1.0 / (envRelSec * sampleRate));

        const double gateAtkSec = 0.004;
        const double gateRelSec = 0.120;
        gateAtk = (float) std::exp (-1.0 / (gateAtkSec * sampleRate));
        gateRel = (float) std::exp (-1.0 / (gateRelSec * sampleRate));

        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 8; ++ch)
        {
            lastGood[ch] = 0.f;
            grEnv[ch]    = 1.f;
        }
        dryEnv = wetEnv = 0.f;
        gateGain = 1.f;
    }

    /** When false, skip soft-peak stage (upstream polisher owns musical ceiling). */
    void setPeakSafetyEnabled (bool enabled) noexcept { peakSafetyEnabled = enabled; }
    bool isPeakSafetyEnabled() const noexcept { return peakSafetyEnabled; }

    /**
        @param dry   Latency-aligned pre-FX reference (same timeline as mixed)
        @param mixed Post dry/wet buffer to clean in-place
    */
    void process (const juce::dsp::AudioBlock<const float>& dry,
                  juce::dsp::AudioBlock<float>& mixed) noexcept
    {
        const auto nS  = mixed.getNumSamples();
        const auto nCh = juce::jmin ((size_t) numChannels, mixed.getNumChannels());
        if (nS == 0 || nCh == 0)
            return;

        const auto dryCh = dry.getNumChannels() > 0 ? dry.getNumChannels() : 0;
        const bool hasDry = dryCh > 0 && dry.getNumSamples() >= nS;

        // Peak region only — leave program dynamics alone until true ceiling
        constexpr float kneeStart = 0.97f;
        constexpr float softCeil  = 0.999f;
        constexpr float residualFloor = 1.0e-4f;

        for (size_t i = 0; i < nS; ++i)
        {
            float dryPeak = 0.f;
            if (hasDry)
            {
                for (size_t ch = 0; ch < juce::jmin (nCh, dryCh); ++ch)
                    dryPeak = juce::jmax (dryPeak, std::abs (dry.getChannelPointer (ch)[i]));
            }
            dryEnv = (dryPeak > dryEnv)
                       ? envAtk * dryEnv + (1.f - envAtk) * dryPeak
                       : envRel * dryEnv + (1.f - envRel) * dryPeak;

            float wetPeak = 0.f;
            for (size_t ch = 0; ch < nCh; ++ch)
                wetPeak = juce::jmax (wetPeak, std::abs (mixed.getChannelPointer (ch)[i]));
            wetEnv = (wetPeak > wetEnv)
                       ? envAtk * wetEnv + (1.f - envAtk) * wetPeak
                       : envRel * wetEnv + (1.f - envRel) * wetPeak;

            // Residual mute ONLY when both paths are at noise floor (not musical tails)
            float targetGate = 1.f;
            if (dryEnv < residualFloor && wetEnv < residualFloor)
                targetGate = 0.f;

            if (targetGate > gateGain)
                gateGain = gateAtk * gateGain + (1.f - gateAtk) * targetGate;
            else
                gateGain = gateRel * gateGain + (1.f - gateRel) * targetGate;
            if (gateGain < 1.0e-4f)
                gateGain = 0.f;

            for (size_t ch = 0; ch < nCh; ++ch)
            {
                float v = mixed.getChannelPointer (ch)[i] * gateGain;

                if (! std::isfinite (v))
                {
                    v = lastGood[ch];
                    hitInvalid = true;
                }

                if (peakSafetyEnabled)
                {
                    const float absV = std::abs (v);
                    float needed = 1.f;
                    if (absV > kneeStart)
                    {
                        const float over = absV - kneeStart;
                        const float room = softCeil - kneeStart;
                        const float compressed = kneeStart
                            + room * std::tanh (over / juce::jmax (1.0e-6f, room));
                        needed = compressed / juce::jmax (absV, 1.0e-12f);
                        if (needed < 0.92f)
                            hitLimiter = true;
                    }

                    float& gr = grEnv[ch];
                    if (needed < gr)
                        gr = limAtk * gr + (1.f - limAtk) * needed;
                    else
                        gr = limRel * gr + (1.f - limRel) * needed;
                    v *= gr;

                    if (v > softCeil)
                        v = softCeil + 0.01f * std::tanh (v - softCeil);
                    else if (v < -softCeil)
                        v = -softCeil + 0.01f * std::tanh (v + softCeil);
                }

                if (std::abs (v) < 1.0e-15f)
                    v = 0.f;

                lastGood[ch] = v;
                mixed.getChannelPointer (ch)[i] = v;
            }
        }
    }

    bool consumeLimiterHit() noexcept  { return hitLimiter.exchange (false); }
    bool consumeInvalid() noexcept     { return hitInvalid.exchange (false); }

private:
    double sampleRate { 44100.0 };
    int numChannels { 2 };

    float limAtk { 0.f }, limRel { 0.f };
    float gateAtk { 0.f }, gateRel { 0.f };
    float envAtk { 0.f }, envRel { 0.f };

    float dryEnv { 0.f };
    float wetEnv { 0.f };
    float gateGain { 1.f };
    bool peakSafetyEnabled { true };
    std::array<float, 8> lastGood {};
    std::array<float, 8> grEnv { 1,1,1,1,1,1,1,1 };

    std::atomic<bool> hitLimiter { false };
    std::atomic<bool> hitInvalid { false };
};
