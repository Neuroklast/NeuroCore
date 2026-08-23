#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include "../../dsp/LookupTables.h"
#include <cmath>

using namespace dsl;

namespace
{
template <int N>
NK_FORCEINLINE float cascade (float y, float a, float* NK_RESTRICT z) noexcept
{
    for (int i = 0; i < N; ++i)
        y = DSPUtils::onePoleAllpassTick (y, a, z[i]);
    return y;
}

NK_FORCEINLINE float cascadeN (float y, float a, float* NK_RESTRICT z, int n) noexcept
{
    switch (n)
    {
        case 2:  return cascade<2>  (y, a, z);
        case 4:  return cascade<4>  (y, a, z);
        case 6:  return cascade<6>  (y, a, z);
        case 8:  return cascade<8>  (y, a, z);
        case 10: return cascade<10> (y, a, z);
        case 12: return cascade<12> (y, a, z);
        default:
            for (int i = 0; i < n; ++i)
                y = DSPUtils::onePoleAllpassTick (y, a, z[i]);
            return y;
    }
}

NK_FORCEINLINE void wrapTwoPi (float& phase, float twoPi) noexcept
{
    while (phase >= twoPi)
        phase -= twoPi;
    while (phase < 0.f)
        phase += twoPi;
}

NK_FORCEINLINE void flushZ (float* NK_RESTRICT z, int n) noexcept
{
    for (int i = 0; i < n; ++i)
        z[i] = DSPUtils::flushDenorm (z[i]);
}
} // namespace

void SignalChain::Phaser::clearRuntimeState() noexcept
{
    phase = 0.f;
    lastYL = lastYR = 0.f;
    lastFc = -1.f;
    apA = 0.f;
    std::fill (zL, zL + kMaxStages, 0.f);
    std::fill (zR, zR + kMaxStages, 0.f);
}

void SignalChain::Phaser::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    invSr = 1.f / sampleRate;
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb, double t)
    {
        sm.reset (sampleRate, t);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (stagesSm, stagesExpr, 6.f, Config::kSmoothingTime);
    snap (rateSm, rateExpr, 0.4f, Config::kModSmoothingTime);
    snap (depthSm, depthExpr, 0.7f, Config::kModSmoothingTime);
    snap (centerSm, centerExpr, 800.f, Config::kModSmoothingTime);
    snap (fbSm, feedbackExpr, 0.3f, Config::kSmoothingTime);
    snap (mixSm, mixExpr, 0.5f, Config::kSmoothingTime);
    varNames.clear();
    if (varPtr != nullptr)
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
    clearRuntimeState();
    nStages = juce::jlimit (2, kMaxStages, (int) std::lround (stagesSm.getCurrentValue()));
    apA = DSPUtils::onePoleAllpassA (centerSm.getCurrentValue(), sampleRate);
    lastFc = centerSm.getCurrentValue();
}

float SignalChain::Phaser::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Phaser::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    for (const auto& n : varNames)
    {
        const float v = *n.first;
        stagesExpr.setVariable (n.second, v);
        rateExpr.setVariable (n.second, v);
        depthExpr.setVariable (n.second, v);
        centerExpr.setVariable (n.second, v);
        feedbackExpr.setVariable (n.second, v);
        mixExpr.setVariable (n.second, v);
    }

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fb;
    };
    stagesSm.setCurrentAndTargetValue (ev (stagesExpr, 6.f));
    rateSm.setTargetValue (ev (rateExpr, 0.4f));
    depthSm.setTargetValue (ev (depthExpr, 0.7f));
    centerSm.setTargetValue (ev (centerExpr, 800.f));
    nStages = juce::jlimit (2, kMaxStages, (int) std::lround (stagesSm.getCurrentValue()));
    const float fb = juce::jlimit (0.f, 0.95f, ev (feedbackExpr, 0.3f));
    const float mix = juce::jlimit (0.f, 1.f, ev (mixExpr, 0.5f));
    const float dry = 1.f - mix;
    fbSm.setCurrentAndTargetValue (fb);
    mixSm.setCurrentAndTargetValue (mix);

    float* NK_RESTRICT L = buffer.getWritePointer (0);
    float* NK_RESTRICT R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float ny = sampleRate * 0.45f;
    const int stride = juce::jmax (1, Config::kFilterCoeffStride);
    const int stages = nStages;

    for (int i = 0; i < nS; )
    {
        const int n = juce::jmin (stride, nS - i);
        rateSm.skip (n);
        depthSm.skip (n);
        centerSm.skip (n);

        const float rate = juce::jlimit (0.f, 20.f, rateSm.getCurrentValue());
        const float depth = juce::jlimit (0.f, 1.5f, depthSm.getCurrentValue());
        const float center = juce::jlimit (40.f, ny, centerSm.getCurrentValue());

        if (rate > 0.001f)
        {
            phase += twoPi * rate * invSr * (float) n;
            wrapTwoPi (phase, twoPi);
        }
        const float lfo = (rate > 0.001f) ? LookupTables::fastSin (phase) : 0.f;
        const float fc = juce::jlimit (40.f, ny,
            center * LookupTables::fastExp (lfo * depth * 1.03972077f));
        if (lastFc < 0.f || std::abs (fc - lastFc) > 0.18f)
        {
            apA = DSPUtils::onePoleAllpassA (fc, sampleRate);
            lastFc = fc;
        }
        const float a = apA;

        flushZ (zL, stages);
        if (R != nullptr)
            flushZ (zR, stages);

        for (int k = 0; k < n; ++k)
        {
            const float xL = L[i + k];
            float yL = cascadeN (DSPUtils::satFb (xL + lastYL * fb), a, zL, stages);
            lastYL = yL;
            L[i + k] = xL * dry + yL * mix;

            if (R != nullptr)
            {
                const float xR = R[i + k];
                float yR = cascadeN (DSPUtils::satFb (xR + lastYR * fb), a, zR, stages);
                lastYR = yR;
                R[i + k] = xR * dry + yR * mix;
            }
        }
        i += n;
    }
}
