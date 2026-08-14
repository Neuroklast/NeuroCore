#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Comp::clearRuntimeState() noexcept
{
    envDb = 0.f;
    hpfLpL = 0.f;
    hpfLpR = 0.f;
    hpfLpL2 = 0.f;
    hpfLpR2 = 0.f;
}

void SignalChain::Comp::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    auto resetSm = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fallback)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fallback);
    };
    resetSm (thrSm, threshold, -12.f);
    resetSm (ratioSm, ratio, 4.f);
    resetSm (atkSm, attack, 0.01f);
    resetSm (relSm, release, 0.1f);
    resetSm (kneeSm, kneeDb, 0.f);
    resetSm (makeupSm, makeupDb, 0.f);
    resetSm (hpfSm, hpfHz, 0.f);
    clearRuntimeState();
    varNames.clear();
    if (varPtr != nullptr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
}

float SignalChain::Comp::computeGrDb (float levelDb, float thrDb, float ratio, float knee) const noexcept
{
    const float slope = 1.f - 1.f / juce::jmax (1.001f, ratio);
    const float over = levelDb - thrDb;
    if (knee <= 0.05f)
        return over > 0.f ? over * slope : 0.f;

    const float half = 0.5f * knee;
    if (over <= -half)
        return 0.f;
    if (over >= half)
        return over * slope;
    const float x = over + half;
    return slope * (x * x) / (2.f * knee);
}

float SignalChain::Comp::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::AudioBuffer<float> one (1, 1);
    one.setSample (0, 0, x);
    processBlock (one);
    return one.getSample (0, 0);
}

void SignalChain::Comp::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    if (varPtr != nullptr)
    {
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            threshold.setVariable (n.second, v);
            ratio.setVariable (n.second, v);
            attack.setVariable (n.second, v);
            release.setVariable (n.second, v);
            kneeDb.setVariable (n.second, v);
            makeupDb.setVariable (n.second, v);
            hpfHz.setVariable (n.second, v);
        }
    }

    auto ev = [] (ExpressionEvaluator& e, float fallback)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fallback;
    };
    thrSm.setTargetValue (ev (threshold, -12.f));
    ratioSm.setTargetValue (ev (ratio, 4.f));
    atkSm.setTargetValue (ev (attack, 0.01f));
    relSm.setTargetValue (ev (release, 0.1f));
    kneeSm.setTargetValue (ev (kneeDb, 0.f));
    makeupSm.setTargetValue (ev (makeupDb, 0.f));
    hpfSm.setTargetValue (ev (hpfHz, 0.f));

    for (int i = 0; i < nS; ++i)
    {
        const float thr = juce::jlimit (-80.f, 0.f, thrSm.getNextValue());
        const float rat = juce::jlimit (1.f, 40.f, ratioSm.getNextValue());
        const float atk = juce::jmax (0.001f, atkSm.getNextValue());
        const float rel = juce::jmax (0.005f, relSm.getNextValue());
        const float knee = juce::jlimit (0.f, 24.f, kneeSm.getNextValue());
        const float makeup = juce::jlimit (-24.f, 24.f, makeupSm.getNextValue());
        const float hpf = juce::jlimit (0.f, 800.f, hpfSm.getNextValue());

        float detL = 0.f, detR = 0.f;
        if (followSidechain)
        {
            if (scL != nullptr && scN > 0)
            {
                const int si = juce::jlimit (0, scN - 1, i);
                detL = scL[si];
                detR = scR != nullptr ? scR[si] : detL;
            }
        }
        else
        {
            detL = buffer.getSample (0, i);
            detR = nCh > 1 ? buffer.getSample (1, i) : detL;
        }

        if (hpf > 8.f)
        {
            const float a = std::exp (-2.f * juce::MathConstants<float>::pi * hpf / sampleRate);
            hpfLpL = a * hpfLpL + (1.f - a) * detL;
            hpfLpR = a * hpfLpR + (1.f - a) * detR;
            detL -= hpfLpL;
            detR -= hpfLpR;
            hpfLpL2 = a * hpfLpL2 + (1.f - a) * detL;
            hpfLpR2 = a * hpfLpR2 + (1.f - a) * detR;
            detL -= hpfLpL2;
            detR -= hpfLpR2;
        }

        const float det = juce::jmax (std::abs (detL), std::abs (detR));
        const float levelDb = juce::Decibels::gainToDecibels (det, -100.f);
        const float grDb = computeGrDb (levelDb, thr, rat, knee);

        const float atkC = 1.f - std::exp (-1.f / (atk * sampleRate));
        const float relC = 1.f - std::exp (-1.f / (rel * sampleRate));
        envDb += ((grDb > envDb) ? atkC : relC) * (grDb - envDb);
        if (! std::isfinite (envDb))
            envDb = 0.f;
        envDb = juce::jlimit (0.f, 60.f, envDb);

        const float g = juce::Decibels::decibelsToGain (-envDb + makeup);
        for (int c = 0; c < nCh; ++c)
            buffer.setSample (c, i, buffer.getSample (c, i) * g);
    }

    if (varPtr != nullptr)
        (*varPtr)["y"] = buffer.getSample (0, nS - 1);
}
