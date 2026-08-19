#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

namespace
{
inline float softCeil (float x, float c) noexcept
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

inline float wrapPi (float x) noexcept
{
    constexpr float kPi = juce::MathConstants<float>::pi;
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    x = std::fmod (x + kPi, kTwoPi);
    if (x < 0.f)
        x += kTwoPi;
    return x - kPi;
}
} // namespace

void SignalChain::Pitch::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) spec.sampleRate;
    fft = std::make_unique<juce::dsp::FFT> (kFftOrder);

    window.resize ((size_t) kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        window[(size_t) i] = 0.5f
                           - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                              * (float) i / (float) kFftSize);

    hop = kHopBase;
    applyTempo (currentBpm > 0.0 ? currentBpm : (double) Config::kDefaultTempo);

    ch[0].ensure();
    ch[1].ensure();
    // smb: rover starts at fftSize - hop (inFifoLatency)
    ch[0].rover = kFftSize - hop;
    ch[1].rover = kFftSize - hop;
    latencySamples = kFftSize - hop;

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
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

void SignalChain::Pitch::applyTempo (double bpm) noexcept
{
    if (bpm > 0.0)
        currentBpm = bpm;
    if (! useSync || sampleRate <= 0.f || currentBpm <= 0.0)
    {
        hop = kHopBase;
        latencySamples = kFftSize - hop;
        return;
    }

    const double ms = (60000.0 / currentBpm) * (double) syncBeats;
    hop = juce::jlimit (64, kFftSize / 2,
                        (int) std::lround (ms * 0.001 * (double) sampleRate));
    latencySamples = kFftSize - hop;
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
    const float expct = juce::MathConstants<float>::twoPi * (float) hopSz / (float) kFftSize;
    const float freqPerBin = sampleRate / (float) kFftSize;
    auto& work = c.fftWork;

    std::fill (work.begin(), work.end(), 0.f);
    for (int i = 0; i < kFftSize; ++i)
        work[(size_t) i] = c.inFifo[(size_t) i] * window[(size_t) i];

    fft->performRealOnlyForwardTransform (work.data(), true);

    for (int k = 0; k < kBins; ++k)
    {
        const float re = work[(size_t) (2 * k)];
        const float im = work[(size_t) (2 * k + 1)];
        const float magn = 2.f * std::sqrt (re * re + im * im);
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

    for (int k = 0; k < kBins; ++k)
    {
        const int index = (int) std::lround ((double) k * (double) binScale);
        if (index < 0 || index >= kBins)
            continue;
        c.synMagn[(size_t) index] += c.anaMagn[(size_t) k];
        c.synFreq[(size_t) index] = c.anaFreq[(size_t) k] * pr;
    }

    for (int k = 0; k < kBins; ++k)
    {
        const float magn = c.synMagn[(size_t) k];
        float tmp = (freqPerBin > 0.f) ? (c.synFreq[(size_t) k] / freqPerBin) : 0.f;
        tmp -= (float) k;
        const float delta = tmp * expct;
        const float phase = c.sumPhase[(size_t) k] + delta + (float) k * expct;
        c.sumPhase[(size_t) k] = phase;
        work[(size_t) (2 * k)] = magn * std::cos (phase);
        work[(size_t) (2 * k + 1)] = magn * std::sin (phase);
    }

    fft->performRealOnlyInverseTransform (work.data());

    // Hann + osamp OLA compensation (smbPitchShift scale).
    const float scale = 2.f / (float) kOsamp;
    for (int i = 0; i < kFftSize; ++i)
        c.outAccum[(size_t) i] += scale * window[(size_t) i] * work[(size_t) i] / (float) kFftSize;

    // Publish next hop; shift OLA accumulator and input FIFO (classic smb).
    for (int k = 0; k < hopSz; ++k)
        c.outFifo[(size_t) k] = c.outAccum[(size_t) k];

    std::move (c.outAccum.begin() + hopSz, c.outAccum.begin() + hopSz + kFftSize, c.outAccum.begin());
    std::fill (c.outAccum.begin() + kFftSize, c.outAccum.end(), 0.f);

    const int latency = kFftSize - hopSz;
    for (int k = 0; k < latency; ++k)
        c.inFifo[(size_t) k] = c.inFifo[(size_t) (k + hopSz)];
}

float SignalChain::Pitch::processSample (Chan& c, float x, float pitchRatio, float formantRatio) noexcept
{
    const int hopSz = juce::jmax (1, hop);
    const int latency = kFftSize - hopSz;

    if (c.rover < latency)
        c.rover = latency;

    c.inFifo[(size_t) c.rover] = x;
    const float wet = c.outFifo[(size_t) (c.rover - latency)];
    ++c.rover;

    if (c.rover >= kFftSize)
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

    for (const auto& n : varNames)
    {
        const float v = *n.first;
        semiExpr.setVariable (n.second, v);
        mixExpr.setVariable (n.second, v);
        formantExpr.setVariable (n.second, v);
        ceilingDb.setVariable (n.second, v);
    }

    const float semi = juce::jlimit (-24.f, 24.f, semiExpr.evaluate (0.f));
    const float mixT = juce::jlimit (0.f, 1.f, mixExpr.evaluate (0.f));
    float formant = formantExpr.evaluate (0.f);
    if (! (formant > 0.f))
        formant = 1.f;
    formant = juce::jlimit (0.25f, 4.f, formant);

    const float ceilDb = ceilingDb.evaluate (0.f);
    if (std::abs (ceilDb - cachedCeil) > 1.0e-5f)
    {
        cachedCeil = ceilDb;
        ceilLin = juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 0.f, ceilDb));
    }

    semiSm.setTargetValue (semi);
    mixSm.setTargetValue (mixT);
    formSm.setTargetValue (formant);
    ceilSm.setTargetValue (ceilLin);

    const float pr = std::pow (2.f, semiSm.getTargetValue() / 12.f);
    const float fr = formSm.getTargetValue();

    float* L = buffer.getWritePointer (0);
    float* R = nc > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float mixV = mixSm.getNextValue();
        const float ceilV = ceilSm.getNextValue();
        semiSm.skip (1);
        formSm.skip (1);

        {
            const float dry = L[i];
            float wet = processSample (ch[0], dry, pr, fr);
            wet = softCeil (wet, ceilV);
            L[i] = dry * (1.f - mixV) + wet * mixV;
        }
        if (R != nullptr)
        {
            const float dry = R[i];
            float wet = processSample (ch[1], dry, pr, fr);
            wet = softCeil (wet, ceilV);
            R[i] = dry * (1.f - mixV) + wet * mixV;
        }
    }
}

float SignalChain::Pitch::process (int channel, float x)
{
    if (fft == nullptr)
        return x;

    for (const auto& n : varNames)
    {
        const float v = *n.first;
        semiExpr.setVariable (n.second, v);
        mixExpr.setVariable (n.second, v);
        formantExpr.setVariable (n.second, v);
        ceilingDb.setVariable (n.second, v);
    }

    const float pr = std::pow (2.f, juce::jlimit (-24.f, 24.f, semiExpr.evaluate (0.f)) / 12.f);
    float fr = formantExpr.evaluate (0.f);
    if (! (fr > 0.f)) fr = 1.f;
    fr = juce::jlimit (0.25f, 4.f, fr);
    const float mixV = juce::jlimit (0.f, 1.f, mixExpr.evaluate (0.f));
    const float ceilDb = juce::jlimit (-24.f, 0.f, ceilingDb.evaluate (0.f));
    const int ci = juce::jlimit (0, 1, channel);
    float wet = processSample (ch[ci], x, pr, fr);
    wet = softCeil (wet, juce::Decibels::decibelsToGain (ceilDb));
    return x * (1.f - mixV) + wet * mixV;
}
