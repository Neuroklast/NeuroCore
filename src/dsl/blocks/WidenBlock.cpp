#include "../SignalChain.h"
#include "../../core/Config.h"
#include <cmath>

using namespace dsl;

void SignalChain::Widen::clearRuntimeState() noexcept
{
    for (int i = 0; i < kNumAp; ++i)
    {
        apL[i].clear();
        apR[i].clear();
    }
    hpX = hpY = 0.f;
    lastBass = -1.f;
    slewClock = 0;
}

void SignalChain::Widen::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    const float srScale = sampleRate / 44100.f;
    static constexpr int kBaseL[kNumAp] = { 113, 337, 557 };
    static constexpr int kBaseR[kNumAp] = { 149, 379, 617 };
    for (int i = 0; i < kNumAp; ++i)
    {
        baseL[i] = juce::jmax (8, (int) std::lround ((float) kBaseL[i] * srScale));
        baseR[i] = juce::jmax (8, (int) std::lround ((float) kBaseR[i] * srScale));
        // Headroom so delay can stretch the AP without realloc/clear (click).
        apL[i].allocate (baseL[i] * 3);
        apR[i].allocate (baseR[i] * 3);
        apL[i].delayLen = baseL[i];
        apR[i].delayLen = baseR[i];
        apL[i].delayTarget = baseL[i];
        apR[i].delayTarget = baseR[i];
    }
    hpX = hpY = 0.f;
    lastBass = -1.f;
    slewClock = 0;

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
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

float SignalChain::Widen::process (int, float x) { return x; }

void SignalChain::Widen::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    for (const auto& vn : varNames)
    {
        const float v = *vn.first;
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
        const float ms = juce::jlimit (0.5f, 40.f, delaySm.getNextValue());
        const float bass = juce::jlimit (60.f, 400.f, bassSm.getNextValue());
        if (std::abs (bass - lastBass) > 0.5f)
        {
            lastBass = bass;
            bassA = std::exp (-2.f * juce::MathConstants<float>::pi * bass / sampleRate);
        }

        if ((slewClock++ & 7) == 0)
        {
            const float scale = juce::jlimit (0.35f, 2.4f, ms / 14.f);
            for (int a = 0; a < kNumAp; ++a)
            {
                apL[a].setDelayTarget (juce::jmax (4, (int) std::lround ((float) baseL[a] * scale)));
                apR[a].setDelayTarget (juce::jmax (4, (int) std::lround ((float) baseR[a] * scale)));
                apL[a].slewDelay();
                apR[a].slewDelay();
            }
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

        float side = 0.5f * (dL - dR);
        const float hp = side - hpX + bassA * hpY;
        hpX = side;
        hpY = (std::abs (hp) < 1.0e-20f) ? 0.f : hp;
        side = hpY * w;
        // |side| > |mid| polarity-flips one channel → image slam + click.
        const float cap = 0.92f * std::abs (mid) + 1.0e-6f;
        side = juce::jlimit (-cap, cap, side);

        float outL = mid + side;
        float outR = mid - side;
        if (! std::isfinite (outL)) outL = inL;
        if (! std::isfinite (outR)) outR = inR;
        L[i] = outL;
        if (R != nullptr)
            R[i] = outR;
    }
}
