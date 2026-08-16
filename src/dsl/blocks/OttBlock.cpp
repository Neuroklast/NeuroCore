#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Ott::clearRuntimeState() noexcept
{
    for (auto& c : ch)
    {
        c.lp1a.reset(); c.lp1b.reset(); c.hp1a.reset(); c.hp1b.reset();
        c.lp2a.reset(); c.lp2b.reset(); c.hp2a.reset(); c.hp2b.reset();
    }
    envDb[0] = envDb[1] = envDb[2] = -80.f;
    lastF1 = -1.f;
    lastF2 = -1.f;
    cachedTime = -1.f;
}

void SignalChain::Ott::applyCoeffs (float f1, float f2) noexcept
{
    const float ny = sampleRate * 0.45f;
    f1 = juce::jlimit (40.f, ny - 80.f, f1);
    f2 = juce::jlimit (f1 + 80.f, ny, f2);
    constexpr float q = 0.70710678f;
    auto lp1 = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, f1, q);
    auto hp1 = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f1, q);
    auto lp2 = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, f2, q);
    auto hp2 = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f2, q);
    for (auto& c : ch)
    {
        *c.lp1a.coefficients = *lp1;
        *c.lp1b.coefficients = *lp1;
        *c.hp1a.coefficients = *hp1;
        *c.hp1b.coefficients = *hp1;
        *c.lp2a.coefficients = *lp2;
        *c.lp2b.coefficients = *lp2;
        *c.hp2a.coefficients = *hp2;
        *c.hp2b.coefficients = *hp2;
    }
    lastF1 = f1;
    lastF2 = f2;
}

void SignalChain::Ott::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    const juce::dsp::ProcessSpec one { spec.sampleRate, spec.maximumBlockSize, 1 };
    for (auto& c : ch)
    {
        c.lp1a.prepare (one); c.lp1b.prepare (one);
        c.hp1a.prepare (one); c.hp1b.prepare (one);
        c.lp2a.prepare (one); c.lp2b.prepare (one);
        c.hp2a.prepare (one); c.hp2b.prepare (one);
    }
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (depthSm, depthExpr, 0.5f);
    snap (timeSm, timeExpr, 0.35f);
    snap (inSm, inExpr, 1.f);
    snap (lowSm, lowExpr, 1.f);
    snap (midSm, midExpr, 1.f);
    snap (highSm, highExpr, 1.f);
    snap (f1Sm, f1Expr, 90.f);
    snap (f2Sm, f2Expr, 3200.f);
    applyCoeffs (f1Sm.getCurrentValue(), f2Sm.getCurrentValue());
    clearRuntimeState();
    applyCoeffs (f1Sm.getCurrentValue(), f2Sm.getCurrentValue());
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
}

float SignalChain::Ott::tailSeconds() const noexcept
{
    float t = timeExpr.evaluate (0.f);
    if (! std::isfinite (t)) t = 0.35f;
    t = juce::jlimit (0.f, 1.f, t);
    return 0.02f + t * 0.32f;
}

float SignalChain::Ott::process (int, float x) { return x; }

void SignalChain::Ott::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    if (varPtr != nullptr)
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            depthExpr.setVariable (n.second, v);
            timeExpr.setVariable (n.second, v);
            inExpr.setVariable (n.second, v);
            lowExpr.setVariable (n.second, v);
            midExpr.setVariable (n.second, v);
            highExpr.setVariable (n.second, v);
            f1Expr.setVariable (n.second, v);
            f2Expr.setVariable (n.second, v);
        }

    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fb;
    };
    depthSm.setTargetValue (ev (depthExpr, 0.5f));
    timeSm.setTargetValue (ev (timeExpr, 0.35f));
    inSm.setTargetValue (ev (inExpr, 1.f));
    lowSm.setTargetValue (ev (lowExpr, 1.f));
    midSm.setTargetValue (ev (midExpr, 1.f));
    highSm.setTargetValue (ev (highExpr, 1.f));
    f1Sm.setTargetValue (ev (f1Expr, 90.f));
    f2Sm.setTargetValue (ev (f2Expr, 3200.f));

    const float f1 = f1Sm.getNextValue();
    const float f2 = f2Sm.getNextValue();
    if (nS > 1)
    {
        f1Sm.skip (nS - 1);
        f2Sm.skip (nS - 1);
    }
    if (std::abs (f1 - lastF1) > 0.8f || std::abs (f2 - lastF2) > 0.8f)
        applyCoeffs (f1, f2);

    const int useCh = juce::jmin (nCh, 2);
    float* dst[2] {};
    for (int c = 0; c < useCh; ++c)
        dst[c] = buffer.getWritePointer (c);

    constexpr float downThr = -16.f;
    constexpr float downRatio = 8.f;
    constexpr float upThr = -30.f;
    constexpr float upRatio = 2.8f;
    constexpr float maxUpDb = 16.f;
    constexpr float noiseFloor = -68.f;
    const float downSlope = 1.f - 1.f / downRatio;
    const float upSlope = 1.f - 1.f / upRatio;

    for (int i = 0; i < nS; ++i)
    {
        const float depth = juce::jlimit (0.f, 1.f, depthSm.getNextValue());
        const float time01 = juce::jlimit (0.f, 1.f, timeSm.getNextValue());
        const float inG = juce::jlimit (0.2f, 8.f, inSm.getNextValue());
        const float amt[3] = {
            juce::jlimit (0.f, 1.4f, lowSm.getNextValue()),
            juce::jlimit (0.f, 1.4f, midSm.getNextValue()),
            juce::jlimit (0.f, 1.4f, highSm.getNextValue())
        };

        if (std::abs (time01 - cachedTime) > 1.0e-4f)
        {
            cachedTime = time01;
            const float atkS = 0.004f + time01 * 0.036f;
            const float relS = 0.028f + time01 * 0.26f;
            atkC = 1.f - std::exp (-1.f / juce::jmax (1.f, atkS * sampleRate));
            relC = 1.f - std::exp (-1.f / juce::jmax (1.f, relS * sampleRate));
        }

        float band[2][3] {};
        float dryL = 0.f, dryR = 0.f;
        for (int c = 0; c < useCh; ++c)
        {
            const float x = std::isfinite (dst[c][i]) ? dst[c][i] * inG : 0.f;
            if (c == 0) dryL = dst[c][i];
            else        dryR = dst[c][i];
            auto& p = ch[c];
            const float lo = p.lp1b.processSample (p.lp1a.processSample (x));
            const float hp1 = p.hp1b.processSample (p.hp1a.processSample (x));
            const float md = p.lp2b.processSample (p.lp2a.processSample (hp1));
            const float hi = p.hp2b.processSample (p.hp2a.processSample (hp1));
            band[c][0] = std::isfinite (lo) ? lo : 0.f;
            band[c][1] = std::isfinite (md) ? md : 0.f;
            band[c][2] = std::isfinite (hi) ? hi : 0.f;
        }
        if (useCh < 2)
            dryR = dryL;

        float wetL = 0.f, wetR = 0.f;
        for (int b = 0; b < 3; ++b)
        {
            const float det = juce::jmax (std::abs (band[0][b]),
                                          useCh > 1 ? std::abs (band[1][b]) : 0.f);
            const float levelDb = juce::Decibels::gainToDecibels (det, -80.f);
            envDb[b] += ((levelDb > envDb[b]) ? atkC : relC) * (levelDb - envDb[b]);
            if (! std::isfinite (envDb[b]))
                envDb[b] = -80.f;
            envDb[b] = juce::jlimit (-80.f, 12.f, envDb[b]);

            const float over = envDb[b] - downThr;
            const float downGr = over > 0.f ? over * downSlope : 0.f;
            float upGr = 0.f;
            if (envDb[b] > noiseFloor)
            {
                const float under = upThr - envDb[b];
                if (under > 0.f)
                    upGr = juce::jmin (maxUpDb, under * upSlope);
            }
            const float g = juce::Decibels::decibelsToGain (-downGr + upGr);
            const float a = amt[b];
            wetL += band[0][b] * ((1.f - a) + a * g);
            if (useCh > 1)
                wetR += band[1][b] * ((1.f - a) + a * g);
        }

        auto finish = [depth] (float dry, float wet) noexcept
        {
            float y = dry * (1.f - depth) + wet * depth;
            if (! std::isfinite (y))
                y = dry;
            else if (std::abs (y) > 1.6f)
                y = 1.6f * std::tanh (y / 1.6f);
            return y;
        };
        dst[0][i] = finish (dryL, wetL);
        if (useCh > 1)
            dst[1][i] = finish (dryR, wetR);
    }
}
