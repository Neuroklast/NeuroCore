#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Utility::clearRuntimeState() noexcept
{
    cachedDb = -1000.f;
    lin = 1.f;
}

void SignalChain::Utility::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (gainSm, gainExpr, 0.f);
    snap (panSm, panExpr, 0.f);
    bindings.prepare (varPtr, { &gainExpr, &panExpr });
    clearRuntimeState();
}

float SignalChain::Utility::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Utility::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();
    auto ev = [] (ExpressionEvaluator& e, float fb)
    {
        const float v = e.evaluateLive (0.f);
        return std::isfinite (v) ? v : fb;
    };
    gainSm.setTargetValue (juce::jlimit (-24.f, 24.f, ev (gainExpr, 0.f)));
    panSm.setTargetValue (juce::jlimit (-1.f, 1.f, ev (panExpr, 0.f)));

    float* L = buffer.getWritePointer (0);
    float* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    constexpr float halfPi = 1.57079633f;

    for (int i = 0; i < nS; ++i)
    {
        const float db = gainSm.getNextValue();
        const float pan = panSm.getNextValue();
        if (std::abs (db - cachedDb) > 1.0e-4f)
        {
            cachedDb = db;
            lin = juce::Decibels::decibelsToGain (db);
            if (! std::isfinite (lin))
                lin = 1.f;
        }
        float g = polarity ? -lin : lin;
        float gL = 1.f;
        float gR = 1.f;
        if (pan <= 0.f)
            gR = std::cos (-pan * halfPi);
        else
            gL = std::cos (pan * halfPi);
        const float xL = std::isfinite (L[i]) ? L[i] : 0.f;
        L[i] = xL * g * gL;
        if (R != nullptr)
        {
            const float xR = std::isfinite (R[i]) ? R[i] : 0.f;
            R[i] = xR * g * gR;
        }
    }
}
