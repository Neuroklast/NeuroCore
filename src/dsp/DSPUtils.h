#pragma once
#include <JuceHeader.h>

namespace DSPUtils
{
    // Convert dB to linear amplitude
    static inline constexpr double dbToLinear(double db) noexcept
    {
        return (db == 0.0) ? 1.0 : std::exp(db * std::log(10.0) / 20.0);
    }

    // Convert linear amplitude to dB. Returns -inf for zero.
    static inline constexpr double linearToDb(double lin) noexcept
    {
        return lin > 0.0 ? 20.0 * std::log10(lin) : -std::numeric_limits<double>::infinity();
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

    // Apply RMS based gain compensation on the mixed output block
    template <typename FloatType>
    static inline void autoGainCompensate(const juce::dsp::AudioBlock<FloatType>& dry,
                                          juce::dsp::AudioBlock<FloatType>& mixed,
                                          juce::SmoothedValue<FloatType>& smoothed,
                                          juce::dsp::Gain<FloatType>& gain)
    {
        const auto inRms  = static_cast<FloatType>(rms(dry, 0));
        auto       outRms = static_cast<FloatType>(rms(mixed, 0));

        if (! std::isfinite(outRms) || outRms <= FloatType(0))
            outRms = FloatType(1e-6);

        auto correction = juce::jlimit<FloatType>(FloatType(0.25), FloatType(2.0), inRms / outRms);
        smoothed.setTargetValue(correction);

        for (size_t i = 0; i < mixed.getNumSamples(); ++i)
        {
            gain.setGainLinear(smoothed.getNextValue());
            auto slice = mixed.getSubBlock(i, 1);
            juce::dsp::ProcessContextReplacing<FloatType> ctx(slice);
            gain.process(ctx);
        }
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

