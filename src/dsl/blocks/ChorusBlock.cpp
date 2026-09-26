#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include "../../dsp/LookupTables.h"
#include <cmath>

using namespace dsl;

namespace
{
constexpr float kRateRatio[4] = { 1.f, 1.07f, 0.93f, 1.13f };
constexpr float kPhase0[4] = { 0.f, 2.09439516f, 4.18879032f, 1.04719758f };

NK_FORCEINLINE void wrapTwoPi (float& phase, float twoPi) noexcept
{
    if (phase >= twoPi)
        phase -= twoPi;
    else if (phase < 0.f)
        phase += twoPi;
}
} // namespace

void SignalChain::Chorus::clearRuntimeState() noexcept
{
    writePos = 0;
    for (int v = 0; v < kMaxVoices; ++v)
        phase[v] = kPhase0[v];
    if (delayL != nullptr && delayN > 0)
        std::fill (delayL, delayL + delayN, 0.f);
    if (delayR != nullptr && delayN > 0)
        std::fill (delayR, delayR + delayN, 0.f);
}

void SignalChain::Chorus::prepare (const juce::dsp::ProcessSpec& spec)
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
    snap (rateSm, rateExpr, 0.6f, Config::kModSmoothingTime);
    snap (depthSm, depthExpr, 0.5f, Config::kModSmoothingTime);
    snap (delaySm, delayExpr, 18.f, Config::kModSmoothingTime);
    snap (mixSm, mixExpr, 0.5f, Config::kSmoothingTime);
    snap (widthSm, widthExpr, 0.7f, Config::kSmoothingTime);
    bindings.prepare (varPtr, { &rateExpr, &depthExpr, &delayExpr, &voicesExpr, &mixExpr, &widthExpr });
    clearRuntimeState();
}

float SignalChain::Chorus::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Chorus::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0 || delayL == nullptr || delayN < 8)
        return;

    bindings.refresh();

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluateLive (0.f);
        return std::isfinite (v) ? v : fb;
    };
    rateSm.setTargetValue (juce::jlimit (0.f, 5.f, ev (rateExpr, 0.6f)));
    depthSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (depthExpr, 0.5f)));
    delaySm.setTargetValue (juce::jlimit (5.f, kMaxDelaySec * 1000.f, ev (delayExpr, 18.f)));
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (mixExpr, 0.5f)));
    widthSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (widthExpr, 0.7f)));
    const int voices = juce::jlimit (2, kMaxVoices, (int) std::lround (ev (voicesExpr, 3.f)));

    float* NK_RESTRICT L = buffer.getWritePointer (0);
    float* NK_RESTRICT R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float maxD = (float) (delayN - 2);
    const float invV = 1.f / (float) voices;

    for (int i = 0; i < nS; ++i)
    {
        const float rate = rateSm.getNextValue();
        const float depth = depthSm.getNextValue();
        const float centerMs = delaySm.getNextValue();
        const float mix = mixSm.getNextValue();
        const float width = widthSm.getNextValue();
        const float dry = 1.f - mix;
        const float phaseOff = width * juce::MathConstants<float>::pi;

        float wetL = 0.f;
        float wetR = 0.f;
        for (int v = 0; v < voices; ++v)
        {
            phase[v] += twoPi * rate * kRateRatio[v] * invSr;
            wrapTwoPi (phase[v], twoPi);
            const float lfoL = LookupTables::fastSin (phase[v]);
            const float msL = juce::jlimit (5.f, kMaxDelaySec * 1000.f,
                                            centerMs * (1.f + depth * 0.4f * lfoL));
            const float dL = juce::jlimit (2.f, maxD, msL * 0.001f * sampleRate);
            wetL += DSPUtils::delayRead (delayL, writePos, dL, delayN);

            if (R != nullptr && delayR != nullptr)
            {
                const float lfoR = LookupTables::fastSin (phase[v] + phaseOff);
                const float msR = juce::jlimit (5.f, kMaxDelaySec * 1000.f,
                                                centerMs * (1.f + depth * 0.4f * lfoR));
                const float dR = juce::jlimit (2.f, maxD, msR * 0.001f * sampleRate);
                wetR += DSPUtils::delayRead (delayR, writePos, dR, delayN);
            }
        }
        wetL *= invV;
        wetR *= invV;

        const float xL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float xR = R != nullptr ? (std::isfinite (R[i]) ? R[i] : 0.f) : xL;
        delayL[writePos] = xL;
        if (delayR != nullptr)
            delayR[writePos] = xR;
        if (++writePos >= delayN)
            writePos = 0;

        L[i] = xL * dry + wetL * mix;
        if (R != nullptr)
            R[i] = xR * dry + wetR * mix;
    }
}
