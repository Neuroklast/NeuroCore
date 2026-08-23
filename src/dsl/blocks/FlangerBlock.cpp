#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include "../../dsp/LookupTables.h"
#include <cmath>

using namespace dsl;

namespace
{
NK_FORCEINLINE void wrapTwoPi (float& phase, float twoPi) noexcept
{
    while (phase >= twoPi)
        phase -= twoPi;
    while (phase < 0.f)
        phase += twoPi;
}
} // namespace

void SignalChain::Flanger::clearRuntimeState() noexcept
{
    phase = 0.f;
    writePos = 0;
    lastDelaySamples = -1.f;
    if (delayL != nullptr && delayN > 0)
        std::fill (delayL, delayL + delayN, 0.f);
    if (delayR != nullptr && delayN > 0)
        std::fill (delayR, delayR + delayN, 0.f);
}

void SignalChain::Flanger::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    invSr = 1.f / sampleRate;
    maxDelaySamples = juce::jmax (8, (int) std::ceil ((double) sampleRate * (double) kMaxDelaySec) + 4);
    delayN = maxDelaySamples;
    delayL = DSPUtils::alignedRing (storageL, delayN);
    delayR = DSPUtils::alignedRing (storageR, delayN);
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb, double t)
    {
        sm.reset (sampleRate, t);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (rateSm, rateExpr, 0.25f, Config::kModSmoothingTime);
    snap (depthSm, depthExpr, 0.7f, Config::kModSmoothingTime);
    snap (delaySm, delayExpr, 2.f, Config::kModSmoothingTime);
    snap (fbSm, feedbackExpr, 0.45f, Config::kSmoothingTime);
    snap (mixSm, mixExpr, 0.5f, Config::kSmoothingTime);
    snap (invertSm, invertExpr, 0.f, Config::kSmoothingTime);
    dSampSm.reset (sampleRate, Config::kModSmoothingTime);
    const float ms0 = juce::jlimit (0.1f, kMaxDelaySec * 1000.f, delaySm.getCurrentValue());
    dSampSm.setCurrentAndTargetValue (ms0 * 0.001f * sampleRate);
    fbLatch = juce::jlimit (0.f, 0.95f, fbSm.getCurrentValue());
    mixLatch = juce::jlimit (0.f, 1.f, mixSm.getCurrentValue());
    varNames.clear();
    if (varPtr != nullptr)
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
    clearRuntimeState();
}

float SignalChain::Flanger::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Flanger::processFrame (float& left, float* right, float pol) noexcept
{
    const float maxD = (float) (delayN - 2);
    const float maxDelayDelta = juce::jmax (1.0f, sampleRate * 0.002f);
    float dSamps = dSampSm.getNextValue();
    if (lastDelaySamples > 0.f)
    {
        const float delta = dSamps - lastDelaySamples;
        if (std::abs (delta) > maxDelayDelta)
            dSamps = lastDelaySamples + std::copysign (maxDelayDelta, delta);
    }
    lastDelaySamples = dSamps;
    dSamps = juce::jlimit (2.f, maxD, dSamps);

    const int wp = writePos;
    const int N = delayN;
    const float fb = fbLatch;
    const float mix = mixLatch;
    const float dry = 1.f - mix;

    const float xL = left;
    float wetL = DSPUtils::delayReadLinear (delayL, wp, dSamps, N) * pol;
    wetL = DSPUtils::flushDenorm (wetL);
    delayL[wp] = DSPUtils::satFb (xL + wetL * fb);
    left = xL * dry + wetL * mix;

    if (right != nullptr && delayR != nullptr)
    {
        const float xR = *right;
        float wetR = DSPUtils::delayReadLinear (delayR, wp, dSamps, N) * pol;
        wetR = DSPUtils::flushDenorm (wetR);
        delayR[wp] = DSPUtils::satFb (xR + wetR * fb);
        *right = xR * dry + wetR * mix;
    }

    if (++writePos >= N)
        writePos = 0;
}

void SignalChain::Flanger::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0 || delayL == nullptr || delayN < 8)
        return;

    for (const auto& n : varNames)
    {
        const float v = *n.first;
        rateExpr.setVariable (n.second, v);
        depthExpr.setVariable (n.second, v);
        delayExpr.setVariable (n.second, v);
        feedbackExpr.setVariable (n.second, v);
        mixExpr.setVariable (n.second, v);
        invertExpr.setVariable (n.second, v);
    }

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fb;
    };
    rateSm.setTargetValue (ev (rateExpr, 0.25f));
    depthSm.setTargetValue (ev (depthExpr, 0.7f));
    delaySm.setTargetValue (ev (delayExpr, 2.f));
    fbLatch = juce::jlimit (0.f, 0.95f, ev (feedbackExpr, 0.45f));
    mixLatch = juce::jlimit (0.f, 1.f, ev (mixExpr, 0.5f));
    fbSm.setCurrentAndTargetValue (fbLatch);
    mixSm.setCurrentAndTargetValue (mixLatch);
    invertSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (invertExpr, 0.f)));

    float* NK_RESTRICT L = buffer.getWritePointer (0);
    float* NK_RESTRICT R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const int stride = juce::jmax (1, Config::kFilterCoeffStride);

    for (int i = 0; i < nS; )
    {
        const int n = juce::jmin (stride, nS - i);
        rateSm.skip (n);
        depthSm.skip (n);
        delaySm.skip (n);
        invertSm.skip (n);

        const float rate = juce::jlimit (0.f, 20.f, rateSm.getCurrentValue());
        const float depth = juce::jlimit (0.f, 1.f, depthSm.getCurrentValue());
        const float centerMs = juce::jlimit (0.1f, kMaxDelaySec * 1000.f, delaySm.getCurrentValue());
        if (rate > 0.001f)
        {
            phase += twoPi * rate * invSr * (float) n;
            wrapTwoPi (phase, twoPi);
        }
        const float lfo = (rate > 0.001f) ? LookupTables::fastSin (phase) : 0.f;
        const float ms = juce::jlimit (0.1f, kMaxDelaySec * 1000.f, centerMs * (1.f + depth * lfo));
        dSampSm.setTargetValue (ms * 0.001f * sampleRate);
        const float pol = 1.f - 2.f * juce::jlimit (0.f, 1.f, invertSm.getCurrentValue());

        for (int k = 0; k < n; ++k)
            processFrame (L[i + k], R != nullptr ? &R[i + k] : nullptr, pol);
        i += n;
    }
}
