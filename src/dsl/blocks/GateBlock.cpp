#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <cmath>

using namespace dsl;

void SignalChain::Gate::clearRuntimeState() noexcept
{
    env = 0.f;
    gain = 0.f;
    holdLeft = 0.f;
    open = false;
    cachedAtk = cachedRel = cachedHyst = -1.f;
    cachedThr = cachedRange = cachedCeil = 1.0e9f;
    ceilLin = 1.f;
}

void SignalChain::Gate::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    thrSm.reset (sampleRate, Config::kSmoothingTime);
    hystSm.reset (sampleRate, Config::kSmoothingTime);
    atkSm.reset (sampleRate, Config::kSmoothingTime);
    holdSm.reset (sampleRate, Config::kSmoothingTime);
    relSm.reset (sampleRate, Config::kSmoothingTime);
    rangeSm.reset (sampleRate, Config::kSmoothingTime);
    ceilSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fallback;
    };
    thrSm.setCurrentAndTargetValue (snap (thresholdDb, -42.f));
    hystSm.setCurrentAndTargetValue (snap (hystDb, 3.f));
    atkSm.setCurrentAndTargetValue (snap (attack, 0.001f));
    holdSm.setCurrentAndTargetValue (snap (hold, 0.04f));
    relSm.setCurrentAndTargetValue (snap (release, 0.08f));
    rangeSm.setCurrentAndTargetValue (snap (rangeDb, -80.f));
    ceilSm.setCurrentAndTargetValue (snap (ceilingDb, 0.f));
    clearRuntimeState();
    gain = juce::Decibels::decibelsToGain (juce::jlimit (-90.f, 0.f, rangeSm.getCurrentValue()));
    ceilLin = juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 0.f, ceilSm.getCurrentValue()));
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

float SignalChain::Gate::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    return x * gain;
}

void SignalChain::Gate::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    for (const auto& n : varNames)
    {
        const float v = *n.first;
        thresholdDb.setVariable (n.second, v);
        hystDb.setVariable (n.second, v);
        attack.setVariable (n.second, v);
        hold.setVariable (n.second, v);
        release.setVariable (n.second, v);
        rangeDb.setVariable (n.second, v);
        ceilingDb.setVariable (n.second, v);
    }

    auto evalOr = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fallback;
    };

    thrSm.setTargetValue (evalOr (thresholdDb, -42.f));
    hystSm.setTargetValue (evalOr (hystDb, 3.f));
    atkSm.setTargetValue (evalOr (attack, 0.001f));
    holdSm.setTargetValue (evalOr (hold, 0.04f));
    relSm.setTargetValue (evalOr (release, 0.08f));
    rangeSm.setTargetValue (evalOr (rangeDb, -80.f));
    ceilSm.setTargetValue (evalOr (ceilingDb, 0.f));

    const float invSr = 1.f / sampleRate;

    float* out[2] {};
    const int useCh = juce::jmin (nCh, 2);
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float thrDb = juce::jlimit (-90.f, 0.f, thrSm.getNextValue());
        const float hyst  = juce::jlimit (0.f, 24.f, hystSm.getNextValue());
        const float atkS  = juce::jlimit (0.0002f, 0.08f, atkSm.getNextValue());
        const float holdS = juce::jlimit (0.f, 1.f, holdSm.getNextValue());
        const float relS  = juce::jlimit (0.002f, 1.f, relSm.getNextValue());
        const float rngDb = juce::jlimit (-90.f, 0.f, rangeSm.getNextValue());
        const float ceilDb = juce::jlimit (-24.f, 0.f, ceilSm.getNextValue());

        if (std::abs (atkS - cachedAtk) > 1.0e-7f)
        {
            cachedAtk = atkS;
            atkC = 1.f - std::exp (-1.f / juce::jmax (1.f, atkS * sampleRate));
        }
        if (std::abs (relS - cachedRel) > 1.0e-7f)
        {
            cachedRel = relS;
            relC = 1.f - std::exp (-1.f / juce::jmax (1.f, relS * sampleRate));
        }
        if (std::abs (thrDb - cachedThr) > 1.0e-4f || std::abs (hyst - cachedHyst) > 1.0e-4f)
        {
            cachedThr = thrDb;
            cachedHyst = hyst;
            openLin = juce::Decibels::decibelsToGain (thrDb);
            closeLin = juce::Decibels::decibelsToGain (thrDb - hyst);
        }
        if (std::abs (rngDb - cachedRange) > 1.0e-4f)
        {
            cachedRange = rngDb;
            rangeLin = juce::Decibels::decibelsToGain (rngDb);
        }
        if (std::abs (ceilDb - cachedCeil) > 1.0e-4f)
        {
            cachedCeil = ceilDb;
            ceilLin = juce::Decibels::decibelsToGain (ceilDb);
        }

        float det = 0.f;
        if (followSidechain && scL != nullptr && scN > 0)
        {
            const int si = juce::jlimit (0, scN - 1, i);
            det = std::abs (scL[si]);
            if (scR != nullptr)
                det = juce::jmax (det, std::abs (scR[si]));
        }
        else
        {
            for (int c = 0; c < useCh; ++c)
                det = juce::jmax (det, std::abs (out[c][i]));
        }

        env += ((det > env) ? atkC : relC) * (det - env);
        if (! std::isfinite (env) || std::abs (env) < 1.0e-20f)
            env = 0.f;

        if (env >= openLin)
        {
            open = true;
            holdLeft = holdS;
        }
        else if (open && holdLeft > 0.f)
        {
            holdLeft -= invSr;
        }
        else if (env <= closeLin)
        {
            open = false;
        }

        const float target = open ? 1.f : rangeLin;
        const float gC = open ? atkC : relC;
        gain += gC * (target - gain);
        if (! std::isfinite (gain))
            gain = target;
        gain = juce::jlimit (0.f, 1.f, gain);

        for (int c = 0; c < useCh; ++c)
            out[c][i] = DSPUtils::softCeilSample (out[c][i] * gain, ceilLin);
    }
}
