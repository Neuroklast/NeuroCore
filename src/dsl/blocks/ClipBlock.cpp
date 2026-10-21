#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

namespace
{
    float snapEval (ExpressionEvaluator& e, float fallback) noexcept
    {
        const float v = e.evaluateLive (0.0f);
        return std::isfinite (v) ? v : fallback;
    }
}

void SignalChain::Clip::clearRuntimeState() noexcept
{
    peakEnv = 0.0f;
    autoGainSm = 1.0f;
    dcX[0] = dcX[1] = 0.0f;
    dcY[0] = dcY[1] = 0.0f;
    cachedDrive = cachedThr = cachedKnee = cachedBias = -1.0e9f;
    cachedTrim = cachedMix = -1.0e9f;
    driveLin = 1.0f;
    thrLin = 1.0f;
    kneeL = 0.0f;
    biasL = 0.0f;
    trimL = 1.0f;
    mixL = 0.0f;
}

float SignalChain::Clip::softClipAt (float x, float thr, float knee) noexcept
{
    if (thr <= 0.0f)
        return 0.0f;
    const float a = thr * (1.0f - knee); // Ende des linearen Bereichs (Kniebeginn)
    const float ax = std::abs (x);
    if (ax <= a)
        return x;
    if (ax >= thr)
        return (x >= 0.0f) ? thr : -thr;
    // Smoothstep zwischen a und thr: C1-stetig, erreicht thr exakt.
    const float t = juce::jlimit (0.0f, 1.0f, (ax - a) / juce::jmax (1.0e-6f, thr - a));
    const float smooth = t * t * (3.0f - 2.0f * t);
    return (x >= 0.0f) ? (a + (thr - a) * smooth) : -(a + (thr - a) * smooth);
}

float SignalChain::Clip::dcBlock (int ch, float x) noexcept
{
    constexpr float R = 0.995f;
    float y = x - dcX[(size_t) ch] + R * dcY[(size_t) ch];
    if (! std::isfinite (y))
        y = 0.0f;
    dcX[(size_t) ch] = x;
    dcY[(size_t) ch] = y;
    return y;
}

void SignalChain::Clip::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    driveSm.reset (sampleRate, Config::kSmoothingTime);
    thrSm.reset (sampleRate, Config::kSmoothingTime);
    kneeSm.reset (sampleRate, Config::kSmoothingTime);
    biasSm.reset (sampleRate, Config::kSmoothingTime);
    trimSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    driveSm.setCurrentAndTargetValue (snap (driveExpr, 0.0f));
    thrSm.setCurrentAndTargetValue (snap (thresholdExpr, 0.0f));
    kneeSm.setCurrentAndTargetValue (snap (kneeExpr, 0.5f));
    biasSm.setCurrentAndTargetValue (snap (biasExpr, 0.0f));
    trimSm.setCurrentAndTargetValue (snap (outputTrimExpr, 0.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.0f));

    // Modus ist diskret: String (hard/soft/asymmetric) oder numerisch 0/1/2,
    // aufgelöst beim Prepare, nicht pro Sample interpoliert.
    const auto modeStr = varPtr != nullptr ? juce::String() : juce::String();
    juce::ignoreUnused (modeStr);
    mode = Mode::Hard;

    // Modus ist diskret (0 = hard, 1 = soft, 2 = asymmetric), aufgelöst beim Prepare.
    const int modeIdx = (int) juce::jlimit (0.0f, 2.0f, snap (modeExpr, 0.0f));
    mode = static_cast<Mode> (modeIdx);

    // Auto Gain: Attack schneller als Release, damit Spitzen erfasst, die
    // Gain-Reduktion aber nicht bei jedem Transienten pumpt.
    const float atkS = 0.005f;
    const float relS = 0.100f;
    agcAtkC = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, atkS * sampleRate));
    agcRelC = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, relS * sampleRate));

    bindings.prepare (varPtr, { &driveExpr, &thresholdExpr, &kneeExpr, &biasExpr,
                                &outputTrimExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Clip::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Clip::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    const bool doDc = (mode == Mode::Asymmetric);

    for (int i = 0; i < nS; ++i)
    {
        const float driveDb = juce::jlimit (-24.0f, 36.0f, snapEval (driveExpr, 0.0f));
        const float thrDb   = juce::jlimit (-36.0f, 0.0f, snapEval (thresholdExpr, 0.0f));
        const float knee    = juce::jlimit (0.0f, 1.0f, snapEval (kneeExpr, 0.5f));
        const float bias    = juce::jlimit (-1.0f, 1.0f, snapEval (biasExpr, 0.0f));
        const float trimLin = juce::Decibels::decibelsToGain (juce::jlimit (-36.0f, 12.0f, snapEval (outputTrimExpr, 0.0f)));
        const float mix     = juce::jlimit (0.0f, 1.0f, snapEval (mixExpr, 0.0f));

        const float driveLin = juce::Decibels::decibelsToGain (driveDb);
        const float thrLin   = juce::Decibels::decibelsToGain (thrDb);

        // Eingangs-Peak (kanalverknüpft) für Auto Gain.
        float inPeak = 0.0f;
        for (int c = 0; c < useCh; ++c)
            inPeak = juce::jmax (inPeak, std::abs (out[c][i]));
        if (! std::isfinite (inPeak))
            inPeak = 0.0f;

        if (inPeak > peakEnv)
            peakEnv += (inPeak - peakEnv) * agcAtkC;
        else
            peakEnv += (inPeak - peakEnv) * agcRelC;
        if (! std::isfinite (peakEnv) || peakEnv < 0.0f)
            peakEnv = 0.0f;

        float agcTarget = 1.0f;
        if (autoGain && peakEnv > thrLin)
            agcTarget = thrLin / juce::jmax (1.0e-4f, peakEnv);
        if (agcTarget < autoGainSm)
            autoGainSm += (agcTarget - autoGainSm) * agcAtkC;
        else
            autoGainSm += (agcTarget - autoGainSm) * agcRelC;
        if (! std::isfinite (autoGainSm))
            autoGainSm = 1.0f;

        const float effDrive = driveLin * autoGainSm;

        for (int c = 0; c < useCh; ++c)
        {
            float dry = out[c][i];
            float x = dry * effDrive;
            x += bias;
            float y;
            if (mode == Mode::Hard)
                y = juce::jlimit (-thrLin, thrLin, x);
            else if (mode == Mode::Soft)
                y = softClipAt (x, thrLin, knee);
            else
            {
                const float posThr = juce::jmax (1.0e-4f, thrLin * (1.0f + bias));
                const float negThr = juce::jmax (1.0e-4f, thrLin * (1.0f - bias));
                if (x >= 0.0f)
                    y = softClipAt (x, posThr, knee);
                else
                    y = -softClipAt (-x, negThr, knee);
            }
            if (doDc)
                y = dcBlock (c, y);
            y *= trimLin;
            out[c][i] = dry * (1.0f - mix) + y * mix;
        }
    }
}
