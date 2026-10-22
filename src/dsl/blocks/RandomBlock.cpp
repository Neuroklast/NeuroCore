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

    float rand01 (uint32_t& seed) noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float> (seed >> 8) / 16777216.0f;
    }
}

void SignalChain::Random::clearRuntimeState() noexcept
{
    phase = 0.0;
    current = 0.0f;
    smoothed = 0.0f;
    rngState = seed;
}

void SignalChain::Random::applyTempo (double bpm) noexcept
{
    if (bpm > 0.0)
        hostBpm = bpm;
}

void SignalChain::Random::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    minSm.reset (sampleRate, Config::kSmoothingTime);
    maxSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    smoothSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 10.0f));
    minSm.setCurrentAndTargetValue (snap (minExpr, -1.0f));
    maxSm.setCurrentAndTargetValue (snap (maxExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    smoothSm.setCurrentAndTargetValue (snap (smoothExpr, 0.0f));
    bindings.prepare (varPtr, { &rateExpr, &minExpr, &maxExpr, &mixExpr, &smoothExpr });
    reserveControlLane (spec);
    clearRuntimeState();
}

float SignalChain::Random::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Random::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nS = buffer.getNumSamples();
    if (nS <= 0)
        return;

    if ((int) modLane.size() < nS)
        modLane.resize ((size_t) nS);

    bindings.refresh();

    rateSm.setTargetValue (snapEval (rateExpr, 10.0f));
    minSm.setTargetValue (snapEval (minExpr, -1.0f));
    maxSm.setTargetValue (snapEval (maxExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));
    smoothSm.setTargetValue (snapEval (smoothExpr, 0.0f));

    const double invSr = 1.0 / (double) sampleRate;
    const double bpm = hostBpm > 0.0 ? hostBpm : (double) Config::kDefaultTempo;
    const double syncHz = syncEnabled
                              ? bpm / (juce::jmax (1.0 / 64.0, (double) syncWhole) * 240.0)
                              : 0.0;

    for (int i = 0; i < nS; ++i)
    {
        const float rate = juce::jlimit (0.0f, 1000.0f, rateSm.getNextValue());
        float lo = juce::jlimit (-10.0f, 10.0f, minSm.getNextValue());
        float hi = juce::jlimit (-10.0f, 10.0f, maxSm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());
        const float smooth = juce::jlimit (0.0f, 1.0f, smoothSm.getNextValue());

        if (lo > hi)
            std::swap (lo, hi);

        const double stepHz = syncEnabled ? syncHz : (double) rate;
        if (stepHz > 0.0)
            phase += stepHz * invSr;

        while (phase >= 1.0)
        {
            phase -= 1.0;
            float r = rand01 (rngState);
            if (shape == Shape::Triangle)
                r = (r + rand01 (rngState)) * 0.5f;
            else if (shape == Shape::Gaussian)
                r = (r + rand01 (rngState) + rand01 (rngState) + rand01 (rngState)) * 0.25f;

            if (bipolar)
            {
                const float level = juce::jmax (std::abs (lo), std::abs (hi));
                current = (2.0f * r - 1.0f) * level;
            }
            else
            {
                current = lo + (hi - lo) * r;
            }
        }

        if (smooth <= 1.0e-4f)
            smoothed = current;
        else
        {
            const float tc = juce::jmax (0.001f, smooth * 0.5f);
            const float coeff = 1.0f - std::exp (-1.0f / (tc * sampleRate));
            smoothed += (current - smoothed) * coeff;
        }

        modLane[(size_t) i] = smoothed * mix;
    }
}
