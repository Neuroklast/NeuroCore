#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Transient::clearRuntimeState() noexcept
{
    fastE = slowE = 0.f;
    cachedFast = cachedSlow = -1.f;
}

void SignalChain::Transient::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    auto snap = [this] (juce::SmoothedValue<float>& sm, ExpressionEvaluator& e, float fb)
    {
        sm.reset (sampleRate, Config::kSmoothingTime);
        const float v = e.evaluate (0.f);
        sm.setCurrentAndTargetValue (std::isfinite (v) ? v : fb);
    };
    snap (atkSm, attackExpr, 0.5f);
    snap (susSm, sustainExpr, 0.f);
    snap (fastSm, fastExpr, 0.002f);
    snap (slowSm, slowExpr, 0.08f);
    snap (mixSm, mixExpr, 1.f);
    bindings.prepare (varPtr, { &attackExpr, &sustainExpr, &fastExpr, &slowExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::Transient::process (int, float x)
{
    float y = x;
    float* chs[1] { &y };
    juce::AudioBuffer<float> one (chs, 1, 1);
    processBlock (one);
    return y;
}

void SignalChain::Transient::processBlock (juce::AudioBuffer<float>& buffer)
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
    atkSm.setTargetValue (juce::jlimit (-1.f, 1.f, ev (attackExpr, 0.5f)));
    susSm.setTargetValue (juce::jlimit (-1.f, 1.f, ev (sustainExpr, 0.f)));
    fastSm.setTargetValue (juce::jlimit (0.0003f, 0.02f, ev (fastExpr, 0.002f)));
    slowSm.setTargetValue (juce::jlimit (0.01f, 0.4f, ev (slowExpr, 0.08f)));
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, ev (mixExpr, 1.f)));

    float* L = buffer.getWritePointer (0);
    float* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < nS; ++i)
    {
        const float attack = atkSm.getNextValue();
        const float sustain = susSm.getNextValue();
        const float fastT = fastSm.getNextValue();
        const float slowT = slowSm.getNextValue();
        const float mix = mixSm.getNextValue();
        if (std::abs (fastT - cachedFast) > 1.0e-6f)
        {
            cachedFast = fastT;
            fastC = 1.f - std::exp (-1.f / (fastT * sampleRate));
        }
        if (std::abs (slowT - cachedSlow) > 1.0e-6f)
        {
            cachedSlow = slowT;
            slowC = 1.f - std::exp (-1.f / (slowT * sampleRate));
        }

        const float xL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float xR = R != nullptr && std::isfinite (R[i]) ? R[i] : xL;
        const float level = juce::jmax (std::abs (xL), std::abs (xR));
        fastE += fastC * (level - fastE);
        slowE += slowC * (level - slowE);
        if (fastE < 1.0e-20f) fastE = 0.f;
        if (slowE < 1.0e-20f) slowE = 0.f;

        const float denom = juce::jmax (slowE, 1.0e-4f);
        float rel = (fastE - slowE) / denom;
        if (! std::isfinite (rel))
            rel = 0.f;
        rel = juce::jlimit (-2.f, 2.f, rel);
        const float pos = juce::jmax (rel, 0.f);
        const float neg = juce::jmax (-rel, 0.f);
        float gain = 1.f + attack * pos + sustain * neg;
        if (! std::isfinite (gain))
            gain = 1.f;
        gain = juce::jlimit (0.f, 4.f, gain);
        const float g = (1.f - mix) + mix * gain;

        L[i] = xL * g;
        if (R != nullptr)
            R[i] = xR * g;
    }
}
