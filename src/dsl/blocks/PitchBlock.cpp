#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <cmath>

using namespace dsl;

namespace
{
inline float wrapPi (float x) noexcept
{
    constexpr float kPi = juce::MathConstants<float>::pi;
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    return x - kTwoPi * std::floor ((x + kPi) / kTwoPi);
}
} // namespace

void SignalChain::Pitch::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = std::isfinite (spec.sampleRate) && spec.sampleRate > 0.0
                     ? (float) spec.sampleRate : 48000.f;
    // Preserve time/frequency resolution when the engine oversamples.
    // Allocation and FFT planning happen only during prepare.
    const int order = juce::jlimit (9, 15, kFftOrder
        + (int) std::lround (std::log2 (sampleRate / 48000.f)));
    fftSize = 1 << order;
    bins = fftSize / 2 + 1;
    fft = std::make_unique<juce::dsp::FFT> (order);

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f
                           - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                              * (float) i / (float) fftSize);

    hop = fftSize / kOsamp;
    applyTempo (currentBpm > 0.0 ? currentBpm : (double) Config::kDefaultTempo);

    ch[0].ensure (fftSize);
    ch[1].ensure (fftSize);
    // smb: rover starts at fftSize - hop (inFifoLatency)
    ch[0].rover = fftSize - hop;
    ch[1].rover = fftSize - hop;
    latencySamples = fftSize;

    semiSm.reset (sampleRate, 0.02);
    mixSm.reset (sampleRate, 0.02);
    formSm.reset (sampleRate, 0.02);
    ceilSm.reset (sampleRate, 0.02);
    semiSm.setCurrentAndTargetValue (0.f);
    mixSm.setCurrentAndTargetValue (1.f);
    formSm.setCurrentAndTargetValue (1.f);
    ceilSm.setCurrentAndTargetValue (1.f);
    cachedCeil = 1.0e9f;
    ceilLin = 1.f;
    bindings.prepare (varPtr, { &semiExpr, &mixExpr, &formantExpr, &ceilingDb });
}

void SignalChain::Pitch::applyTempo (double bpm) noexcept
{
    if (bpm > 0.0)
        currentBpm = bpm;
    // An STFT is a fixed-latency processor, not a tempo-synchronised delay.
    // Retain legacy sync syntax but never alter overlap/latency during playback.
    hop = fftSize / kOsamp;
    latencySamples = fftSize;
}

void SignalChain::Pitch::clearRuntimeState() noexcept
{
    ch[0].clear (hop);
    ch[1].clear (hop);
    semiSm.setCurrentAndTargetValue (semiSm.getTargetValue());
    mixSm.setCurrentAndTargetValue (mixSm.getTargetValue());
    formSm.setCurrentAndTargetValue (formSm.getTargetValue());
    ceilSm.setCurrentAndTargetValue (ceilSm.getTargetValue());
    cachedCeil = 1.0e9f;
}

void SignalChain::Pitch::processFrame (Chan& c, float pitchRatio, float formantRatio) noexcept
{
    const int hopSz = juce::jmax (1, hop);
    const float expct = juce::MathConstants<float>::twoPi * (float) hopSz / (float) fftSize;
    const float freqPerBin = sampleRate / (float) fftSize;
    auto& work = c.fftWork;

    std::fill (work.begin(), work.end(), 0.f);
    for (int i = 0; i < fftSize; ++i)
        work[(size_t) i] = c.inFifo[(size_t) i] * window[(size_t) i];

    fft->performRealOnlyForwardTransform (work.data(), true);

    for (int k = 0; k < bins; ++k)
    {
        const float re = work[(size_t) (2 * k)];
        const float im = work[(size_t) (2 * k + 1)];
        const float magn = std::sqrt (re * re + im * im);
        const float phase = std::atan2 (im, re);

        float delta = phase - c.lastPhase[(size_t) k];
        c.lastPhase[(size_t) k] = phase;
        delta -= (float) k * expct;
        delta = wrapPi (delta);

        c.anaMagn[(size_t) k] = magn;
        c.anaFreq[(size_t) k] = ((float) k + delta / expct) * freqPerBin;
    }

    std::fill (c.synMagn.begin(), c.synMagn.end(), 0.f);
    std::fill (c.synFreq.begin(), c.synFreq.end(), 0.f);

    const float pr = juce::jlimit (0.25f, 4.f, pitchRatio);
    const float fr = juce::jlimit (0.25f, 4.f, formantRatio);
    const float binScale = pr / fr;

    for (int k = 0; k < bins; ++k)
    {
        const int index = (int) std::lround ((double) k * (double) binScale);
        if (index < 0 || index >= bins)
            continue;
        c.synMagn[(size_t) index] += c.anaMagn[(size_t) k];
        c.synFreq[(size_t) index] += c.anaFreq[(size_t) k] * pr * c.anaMagn[(size_t) k];
    }

    for (int k = 0; k < bins; ++k)
    {
        const float magn = c.synMagn[(size_t) k];
        float tmp = (freqPerBin > 0.f) ? (magn > 1.0e-20f ? c.synFreq[(size_t) k] / (magn * freqPerBin) : (float) k) : 0.f;
        tmp -= (float) k;
        const float delta = tmp * expct;
        const float phase = wrapPi (c.sumPhase[(size_t) k] + delta + (float) k * expct);
        c.sumPhase[(size_t) k] = phase;
        work[(size_t) (2 * k)] = magn * std::cos (phase);
        work[(size_t) (2 * k + 1)] = magn * std::sin (phase);
    }

    fft->performRealOnlyInverseTransform (work.data());

    // JUCE inverse FFT includes 1/N. Four overlapping Hann-squared
    // windows sum to 3/2, so unity reconstruction needs exactly 2/3.
    const float scale = 2.f / 3.f;
    for (int i = 0; i < fftSize; ++i)
        c.outAccum[(size_t) i] += scale * window[(size_t) i] * work[(size_t) i];

    // Publish next hop; shift OLA accumulator and input FIFO (classic smb).
    for (int k = 0; k < hopSz; ++k)
        c.outFifo[(size_t) k] = c.outAccum[(size_t) k];

    std::move (c.outAccum.begin() + hopSz, c.outAccum.begin() + hopSz + fftSize, c.outAccum.begin());
    std::fill (c.outAccum.begin() + fftSize, c.outAccum.end(), 0.f);

    const int latency = fftSize - hopSz;
    for (int k = 0; k < latency; ++k)
        c.inFifo[(size_t) k] = c.inFifo[(size_t) (k + hopSz)];
}

float SignalChain::Pitch::processSample (Chan& c, float x, float pitchRatio, float formantRatio) noexcept
{
    const int hopSz = juce::jmax (1, hop);
    const int latency = fftSize - hopSz;

    if (c.rover < latency)
        c.rover = latency;

    c.inFifo[(size_t) c.rover] = x;
    const float wet = c.outFifo[(size_t) (c.rover - latency)];
    ++c.rover;

    if (c.rover >= fftSize)
    {
        processFrame (c, pitchRatio, formantRatio);
        c.rover = latency;
    }

    return wet;
}

void SignalChain::Pitch::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0 || nc <= 0 || fft == nullptr)
        return;

    bindings.refresh();

    const float semi = juce::jlimit (-24.f, 24.f, semiExpr.evaluateLive (0.f));
    const float mixT = juce::jlimit (0.f, 1.f, mixExpr.evaluateLive (0.f));
    float formant = formantExpr.evaluateLive (0.f);
    if (! (formant > 0.f))
        formant = 1.f;
    formant = juce::jlimit (0.25f, 4.f, formant);

    const float ceilDb = ceilingDb.evaluateLive (0.f);
    if (std::abs (ceilDb - cachedCeil) > 1.0e-5f)
    {
        cachedCeil = ceilDb;
        ceilLin = juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 0.f, ceilDb));
    }

    semiSm.setTargetValue (semi);
    mixSm.setTargetValue (mixT);
    formSm.setTargetValue (formant);
    ceilSm.setTargetValue (ceilLin);

    float* L = buffer.getWritePointer (0);
    float* R = nc > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float mixV = mixSm.getNextValue();
        const float ceilV = ceilSm.getNextValue();
        const float semitones = semiSm.getNextValue();
        // Ratio is consumed only when an FFT frame is completed.
        const float pr = ch[0].rover + 1 == fftSize ? std::exp2 (semitones / 12.f) : 1.f;
        float fr = formSm.getNextValue();
        if (! (fr > 0.f))
            fr = 1.f;

        {
            const float dry = L[i];
            float wet = processSample (ch[0], dry, pr, fr);
            wet = DSPUtils::softCeilSample (wet, ceilV);
            L[i] = ch[0].delayDry (dry) * (1.f - mixV) + wet * mixV;
        }
        if (R != nullptr)
        {
            const float dry = R[i];
            float wet = processSample (ch[1], dry, pr, fr);
            wet = DSPUtils::softCeilSample (wet, ceilV);
            R[i] = ch[1].delayDry (dry) * (1.f - mixV) + wet * mixV;
        }
    }
}

float SignalChain::Pitch::process (int channel, float x)
{
    if (fft == nullptr)
        return x;

    bindings.refresh();

    const float pr = std::pow (2.f, juce::jlimit (-24.f, 24.f, semiExpr.evaluateLive (0.f)) / 12.f);
    float fr = formantExpr.evaluateLive (0.f);
    if (! (fr > 0.f)) fr = 1.f;
    fr = juce::jlimit (0.25f, 4.f, fr);
    const float mixV = juce::jlimit (0.f, 1.f, mixExpr.evaluateLive (0.f));
    const float ceilDb = juce::jlimit (-24.f, 0.f, ceilingDb.evaluateLive (0.f));
    const int ci = juce::jlimit (0, 1, channel);
    float wet = processSample (ch[ci], x, pr, fr);
    wet = DSPUtils::softCeilSample (wet, juce::Decibels::decibelsToGain (ceilDb));
    return ch[ci].delayDry (x) * (1.f - mixV) + wet * mixV;
}
