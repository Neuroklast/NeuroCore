#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

namespace
{
inline float hermiteAt (const std::vector<float>& buf, float pos, int N) noexcept
{
    if (N < 4 || buf.size() < (size_t) N)
        return 0.f;
    float p = pos;
    if (p < 0.f) p += (float) N;
    else if (p >= (float) N) p -= (float) N;
    int i1 = (int) p;
    if (i1 >= N) i1 = 0;
    if (i1 < 0) i1 = 0;
    const float f = p - (float) i1;
    const int i0 = (i1 - 1 + N) % N;
    const int i2 = (i1 + 1) % N;
    const int i3 = (i1 + 2) % N;
    const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1];
    const float y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}
} // namespace

void SignalChain::Widen::clearRuntimeState() noexcept
{
    for (int i = 0; i < kNumAp; ++i)
    {
        apL[i].clear();
        apR[i].clear();
    }
    std::fill (haasBuf.begin(), haasBuf.end(), 0.f);
    writePos = 0;
    hpX = hpY = 0.f;
}

void SignalChain::Widen::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    const float srScale = sampleRate / 44100.f;
    static constexpr int baseL[kNumAp] = { 113, 337, 557 };
    static constexpr int baseR[kNumAp] = { 149, 379, 617 };
    for (int i = 0; i < kNumAp; ++i)
    {
        const int nL = juce::jmax (8, (int) std::lround (baseL[i] * srScale));
        const int nR = juce::jmax (8, (int) std::lround (baseR[i] * srScale));
        apL[i].allocate (nL);
        apR[i].allocate (nR);
        apL[i].delayLen = nL - 1;
        apR[i].delayLen = nR - 1;
    }
    maxDelayN = juce::jmax (16, (int) std::ceil (sampleRate * kMaxHaasSec) + 4);
    haasBuf.assign ((size_t) maxDelayN, 0.f);
    writePos = 0;
    hpX = hpY = 0.f;
    lastBass = -1.f;

    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (widthSm, widthExpr, 0.7f);
    snap (delaySm, delayMs, 14.f);
    snap (bassSm, bassHz, 140.f);
    varNames.clear();
    if (varPtr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
}

float SignalChain::Widen::process (int, float x) { return x; }

void SignalChain::Widen::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0 || maxDelayN < 4)
        return;

    if (varPtr)
        for (const auto& vn : varNames)
        {
            const float v = (*varPtr)[vn.first];
            widthExpr.setVariable (vn.second, v);
            delayMs.setVariable (vn.second, v);
            bassHz.setVariable (vn.second, v);
        }

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fb;
    };
    widthSm.setTargetValue (ev (widthExpr, 0.7f));
    delaySm.setTargetValue (ev (delayMs, 14.f));
    bassSm.setTargetValue (ev (bassHz, 140.f));

    float* L = buffer.getWritePointer (0);
    float* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < nS; ++i)
    {
        const float w = juce::jlimit (0.f, 1.4f, widthSm.getNextValue());
        const float ms = juce::jlimit (0.5f, kMaxHaasSec * 1000.f, delaySm.getNextValue());
        const float bass = juce::jlimit (60.f, 400.f, bassSm.getNextValue());
        if (std::abs (bass - lastBass) > 0.5f)
        {
            lastBass = bass;
            bassA = std::exp (-2.f * juce::MathConstants<float>::pi * bass / sampleRate);
        }

        const float inL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float inR = R != nullptr ? (std::isfinite (R[i]) ? R[i] : 0.f) : inL;
        const float mid = 0.5f * (inL + inR);

        float dL = mid, dR = mid;
        for (int a = 0; a < kNumAp; ++a)
        {
            dL = apL[a].process (dL);
            dR = apR[a].process (dR);
        }

        haasBuf[(size_t) writePos] = dR;
        const float dSamps = juce::jlimit (4.f, (float) (maxDelayN - 4), ms * 0.001f * sampleRate);
        dR = hermiteAt (haasBuf, (float) writePos - dSamps, maxDelayN);
        if (++writePos >= maxDelayN)
            writePos = 0;

        float side = 0.5f * (dL - dR);
        const float hp = side - hpX + bassA * hpY;
        hpX = side;
        hpY = (std::abs (hp) < 1.0e-20f) ? 0.f : hp;
        side = hpY * w;

        float outL = mid + side;
        float outR = mid - side;
        if (! std::isfinite (outL)) outL = inL;
        if (! std::isfinite (outR)) outR = inR;
        if (std::abs (outL) > 1.5f) outL = 1.5f * std::tanh (outL / 1.5f);
        if (std::abs (outR) > 1.5f) outR = 1.5f * std::tanh (outR / 1.5f);
        L[i] = outL;
        if (R != nullptr)
            R[i] = outR;
    }
}
