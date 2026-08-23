#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include "../core/Config.h"
#include <cmath>
#include <cstdint>
#include <vector>
#if defined(_MSC_VER) && defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace DSPUtils
{
    /** True when `p` sits on a `Config::kDspAlign` boundary. */
    NK_FORCEINLINE static bool isAligned64 (const void* p) noexcept
    {
        return p != nullptr
            && (reinterpret_cast<std::uintptr_t> (p) & (Config::kDspAlign - 1)) == 0;
    }

    NK_FORCEINLINE static void prefetchRead (const void* p) noexcept
    {
        if (p == nullptr)
            return;
#if defined(_MSC_VER) && defined(_M_X64)
        _mm_prefetch (static_cast<const char*> (p), _MM_HINT_T0);
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
        __builtin_prefetch (p, 0, 3);
#else
        (void) p;
#endif
    }

    NK_FORCEINLINE static int simdCount() noexcept
    {
        return (int) juce::dsp::SIMDRegister<float>::SIMDNumElements;
    }

    /** Round length up so a SIMD store/load covers the wrap without a ragged tail. */
    NK_FORCEINLINE static int simdPadded (int n) noexcept
    {
        const int w = simdCount();
        return juce::jmax (w, ((juce::jmax (1, n) + w - 1) / w) * w);
    }

    NK_FORCEINLINE static float* alignPointer (std::vector<float>& storage, int logicalN, int extra) noexcept
    {
        const int cap = juce::jmax (4, logicalN);
        const int pad = (int) (Config::kDspAlign / sizeof (float)) + juce::jmax (0, extra);
        storage.assign ((size_t) cap + (size_t) pad, 0.f);
        const auto raw = reinterpret_cast<std::uintptr_t> (storage.data());
        const auto mask = (std::uintptr_t) (Config::kDspAlign - 1);
        return reinterpret_cast<float*> ((raw + mask) & ~mask);
    }

    /**
        Grow `storage` so a cache-line-aligned ring of `n` floats fits.
        Wrap length is SIMD-padded `n` (padding beyond that is only for the pointer).
        Audio-thread callers must not call this — prepare only.
    */
    NK_FORCEINLINE static float* alignedRing (std::vector<float>& storage, int n) noexcept
    {
        const int cap = simdPadded (n);
        return alignPointer (storage, cap, simdCount());
    }

    /** LUT: logical length `n`, 64-byte pointer, SIMD-lane tail so the last tap vector-loads. */
    NK_FORCEINLINE static float* alignedTable (std::vector<float>& storage, int n) noexcept
    {
        return alignPointer (storage, juce::jmax (2, n), simdCount());
    }

    /** Linear LUT tap on a raw table — no vector in the inner loop. */
    NK_FORCEINLINE static float flushDenorm (float x) noexcept
    {
        return (std::abs (x) < 1.0e-20f) ? 0.f : x;
    }

    /** Bilinear 1-pole allpass coefficient for cutoff `fcHz`. */
    NK_FORCEINLINE static float onePoleAllpassA (float fcHz, float sampleRate) noexcept
    {
        const float ny = sampleRate * 0.45f;
        fcHz = juce::jlimit (20.f, ny, fcHz);
        const float w = juce::MathConstants<float>::pi * fcHz / sampleRate;
        const float g = std::tan (juce::jlimit (1.0e-5f, 1.55f, w));
        // (g-1)/(g+1) < 0 below sr/4 — that's the −90° point. (1-g)/(1+g) sits near Nyquist.
        return (g - 1.f) / (g + 1.f);
    }

    NK_FORCEINLINE static float onePoleAllpassTick (float x, float a, float& z) noexcept
    {
        const float y = a * x + z;
        z = x - a * y;
        return y;
    }

    /** Linear delay tap. Same wrap as `Delay::delayRead` — no Hermite across index 0.
        Hot path: `buf` live, `N >= 4`. */
    NK_FORCEINLINE static float delayReadLinear (const float* NK_RESTRICT buf, int writePos,
                                                float delaySamps, int N) noexcept
    {
        prefetchRead (buf + ((writePos + 16) % N));
        const float d = juce::jlimit (2.0f, (float) (N - 2), delaySamps);
        const int di = (int) d;
        const float f = d - (float) di;
        int i0 = writePos - di;
        if (i0 < 0)
            i0 += N;
        int i1 = i0 - 1;
        if (i1 < 0)
            i1 += N;
        return buf[i0] + f * (buf[i1] - buf[i0]);
    }

    /** Feedback limiter. Transparent below 1.5 — same threshold as Delay. */
    NK_FORCEINLINE static float satFb (float x) noexcept
    {
        if (std::abs (x) <= 1.5f)
            return x;
        return 1.5f * std::tanh (x * (1.f / 1.5f));
    }

    NK_FORCEINLINE static float lutInterp (const float* NK_RESTRICT table, int size, float pos) noexcept
    {
        if (table == nullptr || size < 2)
            return 0.f;
        const int idx = juce::jlimit (0, size - 1, (int) pos);
        const int next = idx + 1 < size ? idx + 1 : 0;
        const float frac = pos - (float) (int) pos;
        return table[idx] + frac * (table[next] - table[idx]);
    }

    // Convert dB to linear amplitude
    static inline constexpr double dbToLinear(double db) noexcept
    {
        return (db == 0.0) ? 1.0 : std::exp(db * std::log(10.0) / 20.0);
    }

    /** Atan soft-clip used by engine sanitation (same family as DSL softclip). */
    NK_FORCEINLINE static float sanitationSoftClip (float x) noexcept
    {
        constexpr float k = 1.57079632679f;
        constexpr float s = 0.63661977237f;
        return s * std::atan (k * juce::jlimit (-40.0f, 40.0f, x));
    }

    /** Soft asymptotic ceiling: transparent below |c|, gently folds overs. */
    static inline float softCeilSample (float x, float c) noexcept
    {
        if (c <= 1.0e-6f)
            return 0.f;
        const float a = std::abs (x);
        if (a <= c)
            return x;
        const float over = a - c;
        const float shaped = c + over / (1.f + over / juce::jmax (c, 1.0e-3f));
        return std::copysign (shaped, x);
    }

    // Convert linear amplitude to dB. Returns -inf for zero.
    static inline constexpr double linearToDb(double lin) noexcept
    {
        return lin > 0.0 ? 20.0 * std::log10(lin) : -std::numeric_limits<double>::infinity();
    }

    /** VU-style one-pole in dB. Attack is faster than release so peaks show, falls don't twitch. */
    static inline float smoothMeterDb (float previous, float instant,
                                       float dtSec, float attackSec, float releaseSec) noexcept
    {
        if (! std::isfinite (instant))
            instant = -100.f;
        instant = juce::jlimit (-100.f, 12.f, instant);
        if (! std::isfinite (previous))
            previous = instant;

        dtSec = juce::jmax (1.0e-6f, dtSec);
        const float tau = (instant > previous)
                            ? juce::jmax (1.0e-4f, attackSec)
                            : juce::jmax (1.0e-4f, releaseSec);
        const float coeff = 1.f - std::exp (-dtSec / tau);
        return previous + (instant - previous) * coeff;
    }

    // Kahan summation for improved precision
    template <typename T>
    static inline double sumSquaresKahan(const T* data, size_t numSamples) noexcept
    {
        double sum = 0.0;
        double c = 0.0;
        for (size_t i = 0; i < numSamples; ++i)
        {
            double y = static_cast<double>(data[i]) * static_cast<double>(data[i]) - c;
            double t = sum + y;
            c = (t - sum) - y;
            sum = t;
        }
        return sum;
    }

    // Calculate RMS using Kahan summation
    template <typename T>
    static inline double rms(const T* data, size_t numSamples) noexcept
    {
        if (numSamples == 0)
            return 0.0;
        double sum = sumSquaresKahan(data, numSamples);
        return std::sqrt(sum / static_cast<double>(numSamples));
    }

    // AudioBuffer overload
    template <typename FloatType>
    static inline double rms(const juce::AudioBuffer<FloatType>& buffer,
                             int channel,
                             int startSample,
                             int numSamples) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, buffer.getNumChannels()));
        jassert(startSample >= 0 && startSample + numSamples <= buffer.getNumSamples());
        return rms(buffer.getReadPointer(channel, startSample), static_cast<size_t>(numSamples));
    }

    // AudioBlock overload - entire block of one channel
    template <typename FloatType>
    static inline double rms(const juce::dsp::AudioBlock<FloatType>& block,
                             int channel) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, static_cast<int>(block.getNumChannels())));
        return rms(block.getChannelPointer(static_cast<size_t>(channel)),
                   static_cast<size_t>(block.getNumSamples()));
    }

    // Detect DC offset via running mean using Kahan summation
    template <typename T>
    static inline double detectDCOffset(const T* data, size_t numSamples) noexcept
    {
        if (numSamples == 0)
            return 0.0;
        double mean = 0.0;
        double c = 0.0;
        for (size_t i = 0; i < numSamples; ++i)
        {
            double y = static_cast<double>(data[i]) - c;
            double t = mean + y;
            c = (t - mean) - y;
            mean = t;
        }
        return mean / static_cast<double>(numSamples);
    }

    template <typename FloatType>
    static inline double detectDCOffset(const juce::AudioBuffer<FloatType>& buffer,
                                        int channel,
                                        int startSample,
                                        int numSamples) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, buffer.getNumChannels()));
        jassert(startSample >= 0 && startSample + numSamples <= buffer.getNumSamples());
        return detectDCOffset(buffer.getReadPointer(channel, startSample), static_cast<size_t>(numSamples));
    }

    template <typename FloatType>
    static inline double detectDCOffset(const juce::dsp::AudioBlock<FloatType>& block,
                                        int channel) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, static_cast<int>(block.getNumChannels())));
        return detectDCOffset(block.getChannelPointer(static_cast<size_t>(channel)),
                              static_cast<size_t>(block.getNumSamples()));
    }

    // Measure dynamic range in dB: 20*log10(peak / RMS)
    template <typename T>
    static inline double measureDynamicRange(const T* data, size_t numSamples) noexcept
    {
        if (numSamples == 0)
            return 0.0;
        double rmsVal = rms(data, numSamples);
        double peak = 0.0;
        for (size_t i = 0; i < numSamples; ++i)
            peak = std::max(peak, std::abs(static_cast<double>(data[i])));
        return peak > 0.0 ? linearToDb(peak / rmsVal) : 0.0;
    }

    template <typename FloatType>
    static inline double measureDynamicRange(const juce::AudioBuffer<FloatType>& buffer,
                                             int channel,
                                             int startSample,
                                             int numSamples) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, buffer.getNumChannels()));
        jassert(startSample >= 0 && startSample + numSamples <= buffer.getNumSamples());
        return measureDynamicRange(buffer.getReadPointer(channel, startSample),
                                   static_cast<size_t>(numSamples));
    }

    template <typename FloatType>
    static inline double measureDynamicRange(const juce::dsp::AudioBlock<FloatType>& block,
                                             int channel) noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, static_cast<int>(block.getNumChannels())));
        return measureDynamicRange(block.getChannelPointer(static_cast<size_t>(channel)),
                                   static_cast<size_t>(block.getNumSamples()));
    }

    /**
        Architecture: loudness match only — never mute, never residual-gate.

        Residual silence is OutputSanitizer's sole responsibility.

        Contract: `dry` MUST be latency-aligned with `mixed` (LatencyAlignedSidechain).
        @param strength01  0 = off (unity), 1 = full mild match.
    */
    template <typename FloatType>
    static inline void autoGainCompensate(const juce::dsp::AudioBlock<FloatType>& dry,
                                          juce::dsp::AudioBlock<FloatType>& mixed,
                                          juce::SmoothedValue<FloatType>& smoothed,
                                          juce::dsp::Gain<FloatType>& gain,
                                          FloatType strength01 = FloatType(1))
    {
        juce::ignoreUnused(gain);

        const auto strength = juce::jlimit(FloatType(0), FloatType(1), strength01);

        const auto inRms  = static_cast<FloatType>(rms(dry, 0));
        auto       outRms = static_cast<FloatType>(rms(mixed, 0));

        if (! std::isfinite(outRms) || outRms < FloatType(1.0e-12))
            outRms = FloatType(1.0e-12);

        if (strength <= FloatType(1.0e-6)
            || ! std::isfinite(inRms) || inRms < FloatType(1.0e-12))
        {
            smoothed.setTargetValue(FloatType(1));
        }
        else
        {
            auto full = inRms / outRms;
            if (! std::isfinite(full))
                full = FloatType(1);

            const FloatType blend = (full >= FloatType(1))
                                        ? FloatType(0.25)
                                        : FloatType(0.08);
            auto correction = FloatType(1) + (full - FloatType(1)) * blend * strength;
            correction = juce::jlimit<FloatType>(FloatType(0.75), FloatType(1.6), correction);
            // When strength < 1, pull correction toward unity
            correction = FloatType(1) + (correction - FloatType(1)) * strength;
            smoothed.setTargetValue(correction);
        }

        const auto numSamples  = mixed.getNumSamples();
        const auto numChannels = mixed.getNumChannels();
        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto g = smoothed.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
                mixed.getChannelPointer(ch)[i] *= g;
        }
    }

    /**
        Sample-accurate dry/wet crossfade (linear rule).
        dry and wet must share timeline (latency-aligned). wetInOut is overwritten.
        wStart/wEnd are proportions at first/last sample of the block.
    */
    template <typename FloatType>
    static inline void mixDryWetContinuous(const juce::AudioBuffer<FloatType>& dry,
                                           juce::AudioBuffer<FloatType>& wetInOut,
                                           FloatType wStart,
                                           FloatType wEnd) noexcept
    {
        const int nS  = wetInOut.getNumSamples();
        const int nCh = juce::jmin (dry.getNumChannels(), wetInOut.getNumChannels());
        if (nS <= 0 || nCh <= 0)
            return;

        const FloatType denom = nS > 1 ? FloatType(nS - 1) : FloatType(1);
        for (int i = 0; i < nS; ++i)
        {
            const FloatType t = FloatType(i) / denom;
            const FloatType w = juce::jlimit(FloatType(0), FloatType(1),
                                             wStart + (wEnd - wStart) * t);
            const FloatType dG = FloatType(1) - w;
            for (int ch = 0; ch < nCh; ++ch)
            {
                const FloatType d = dry.getSample (ch, i);
                const FloatType v = wetInOut.getSample (ch, i);
                wetInOut.setSample (ch, i, d * dG + v * w);
            }
        }
    }

    /** Buffer form for tests: write into separate out. */
    template <typename FloatType>
    static inline void mixDryWetContinuous(const juce::AudioBuffer<FloatType>& dry,
                                           const juce::AudioBuffer<FloatType>& wet,
                                           juce::AudioBuffer<FloatType>& out,
                                           FloatType wStart,
                                           FloatType wEnd) noexcept
    {
        const int nS  = juce::jmin (out.getNumSamples(), wet.getNumSamples());
        const int nCh = juce::jmin (out.getNumChannels(),
                                    juce::jmin (dry.getNumChannels(), wet.getNumChannels()));
        if (nS <= 0 || nCh <= 0)
            return;
        for (int ch = 0; ch < nCh; ++ch)
            out.copyFrom (ch, 0, wet, ch, 0, nS);
        mixDryWetContinuous (dry, out, wStart, wEnd);
    }

    /** Equal-power mix. Polarity follows dry/wet correlation so 50% inverted signals
        do not null. Mix 0 is dry, mix 1 is wet unchanged. */
    struct DryWetMixState
    {
        float polarity { 1.f };
        float corrAbs { 1.f };
    };

    /** Mastering insert mix: linear when dry/wet are correlated (loudness-neutral),
        equal-power when uncorrelated, polarity latch so anti-phase does not null.
        Mix 0 is dry, mix 1 is wet unchanged. */
    template <typename FloatType>
    static inline void mixDryWetPhaseFree (const juce::AudioBuffer<FloatType>& dry,
                                           juce::AudioBuffer<FloatType>& wetInOut,
                                           FloatType wStart,
                                           FloatType wEnd,
                                           DryWetMixState& st) noexcept
    {
        const int nS  = wetInOut.getNumSamples();
        const int nCh = juce::jmin (dry.getNumChannels(), wetInOut.getNumChannels());
        if (nS <= 0 || nCh <= 0)
            return;

        double accDW = 0.0, accD2 = 0.0, accW2 = 0.0;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const FloatType* d = dry.getReadPointer (ch);
            const FloatType* v = wetInOut.getReadPointer (ch);
            for (int i = 0; i < nS; ++i)
            {
                const double a = (double) d[i];
                const double b = (double) v[i];
                accDW += a * b;
                accD2 += a * a;
                accW2 += b * b;
            }
        }
        const double den = std::sqrt (accD2 * accW2);
        const float r = den > 1.0e-12 ? (float) (accDW / den) : 1.f;
        float polTarget = st.polarity >= 0.f ? 1.f : -1.f;
        if (r < -0.35f)
            polTarget = -1.f;
        else if (r > 0.2f)
            polTarget = 1.f;
        st.polarity += 0.12f * (polTarget - st.polarity);
        st.polarity = juce::jlimit (-1.f, 1.f, st.polarity);
        st.corrAbs += 0.18f * (std::abs (r) - st.corrAbs);
        st.corrAbs = juce::jlimit (0.f, 1.f, st.corrAbs);

        const FloatType denom = nS > 1 ? FloatType (nS - 1) : FloatType (1);
        const FloatType halfPi = FloatType (1.5707963267948966);
        const float aLin = st.corrAbs * st.corrAbs;
        for (int i = 0; i < nS; ++i)
        {
            const FloatType t = FloatType (i) / denom;
            const FloatType w = juce::jlimit (FloatType (0), FloatType (1),
                                              wStart + (wEnd - wStart) * t);
            const FloatType gDlin = FloatType (1) - w;
            const FloatType gWlin = w;
            const FloatType theta = w * halfPi;
            const FloatType gDpow = std::cos (theta);
            const FloatType gWpow = std::sin (theta);
            const FloatType gD = FloatType (aLin) * gDlin + FloatType (1 - aLin) * gDpow;
            const FloatType gW = FloatType (aLin) * gWlin + FloatType (1 - aLin) * gWpow;
            const float wm = (float) (w * (FloatType (1) - w) * FloatType (4));
            const FloatType p = FloatType (1) + FloatType (st.polarity - 1.f) * FloatType (wm);
            for (int ch = 0; ch < nCh; ++ch)
            {
                const FloatType d = dry.getSample (ch, i);
                const FloatType v = wetInOut.getSample (ch, i);
                wetInOut.setSample (ch, i, d * gD + v * p * gW);
            }
        }
    }

    template <typename FloatType>
    static inline void mixDryWetPhaseFree (const juce::AudioBuffer<FloatType>& dry,
                                           const juce::AudioBuffer<FloatType>& wet,
                                           juce::AudioBuffer<FloatType>& out,
                                           FloatType wStart,
                                           FloatType wEnd,
                                           DryWetMixState& st) noexcept
    {
        const int nS  = juce::jmin (out.getNumSamples(), wet.getNumSamples());
        const int nCh = juce::jmin (out.getNumChannels(),
                                    juce::jmin (dry.getNumChannels(), wet.getNumChannels()));
        if (nS <= 0 || nCh <= 0)
            return;
        for (int ch = 0; ch < nCh; ++ch)
            out.copyFrom (ch, 0, wet, ch, 0, nS);
        mixDryWetPhaseFree (dry, out, wStart, wEnd, st);
    }

    // Perform FFT analysis on a single channel. Magnitudes will contain fftSize/2 values.
    template <typename FloatType>
    static inline void analyseFFT(const juce::dsp::AudioBlock<FloatType>& block,
                                  int channel,
                                  std::vector<FloatType>& magnitudes,
                                  int order)
    {
        jassert(juce::isPositiveAndBelow(channel, static_cast<int>(block.getNumChannels())));

        const size_t fftSize = static_cast<size_t>(1u << order);
        juce::dsp::FFT fft(order);

        std::vector<FloatType> data(fftSize * 2, FloatType(0));
        const auto num = juce::jmin(static_cast<size_t>(block.getNumSamples()), fftSize);
        std::copy(block.getChannelPointer(static_cast<size_t>(channel)),
                  block.getChannelPointer(static_cast<size_t>(channel)) + num,
                  data.begin());

        fft.performRealOnlyForwardTransform(data.data());

        magnitudes.resize(fftSize / 2);
        for (size_t i = 0; i < fftSize / 2; ++i)
        {
            auto re = data[2 * i];
            auto im = data[2 * i + 1];
            magnitudes[i] = std::sqrt(re * re + im * im);
        }

#if JUCE_DEBUG
        juce::StringArray dbgVals;
        for (size_t i = 0; i < juce::jmin<size_t>(magnitudes.size(), 8); ++i)
            dbgVals.add(juce::String(magnitudes[i], 2));
        DBG("FFT magnitudes: " << dbgVals.joinIntoString(", "));
#endif
    }

    // Calculate approximate integrated LUFS of one channel.
    template <typename FloatType>
    static inline double calculateLufs(const juce::dsp::AudioBlock<FloatType>& block,
                                       int channel,
                                       double sampleRate)
    {
        jassert(juce::isPositiveAndBelow(channel, static_cast<int>(block.getNumChannels())));

        juce::AudioBuffer<FloatType> temp(1, static_cast<int>(block.getNumSamples()));
        temp.copyFrom(0, 0, block.getChannelPointer(static_cast<size_t>(channel)),
                      static_cast<int>(block.getNumSamples()));

        auto tempBlock = juce::dsp::AudioBlock<FloatType>(temp);

        auto hp = juce::dsp::IIR::Filter<FloatType>(
            juce::dsp::IIR::Coefficients<FloatType>::makeHighPass(sampleRate, 40.0));
        auto shelf = juce::dsp::IIR::Filter<FloatType>(
            juce::dsp::IIR::Coefficients<FloatType>::makeHighShelf(sampleRate, 1500.0, 0.7071f,
                                                                  juce::Decibels::decibelsToGain(4.0f)));

        hp.process(juce::dsp::ProcessContextReplacing<FloatType>(tempBlock));
        shelf.process(juce::dsp::ProcessContextReplacing<FloatType>(tempBlock));

        const auto loudness = linearToDb(static_cast<double>(rms(tempBlock, 0)));

#if JUCE_DEBUG
        DBG("LUFS: " << loudness);
#endif
        return loudness;
    }
}
