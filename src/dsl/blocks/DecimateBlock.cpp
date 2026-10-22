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

void SignalChain::Decimate::clearRuntimeState() noexcept
{
    holdL = holdR = 0.0f;
    holdCounter = 0;
    nextHoldSamples = 1;
    jitterState = 0x9e3779b9u;
    lastSampleL = 0.0f;
    lastSampleR = 0.0f;
}

void SignalChain::Decimate::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    bitsSm.reset (sampleRate, Config::kSmoothingTime);
    jitterSm.reset (sampleRate, Config::kSmoothingTime);
    holdSm.reset (sampleRate, Config::kSmoothingTime);
    reconSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    outputSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 0.5f));
    bitsSm.setCurrentAndTargetValue (snap (bitsExpr, 8.0f));
    jitterSm.setCurrentAndTargetValue (snap (jitterExpr, 0.0f));
    holdSm.setCurrentAndTargetValue (snap (holdExpr, 0.0f));
    reconSm.setCurrentAndTargetValue (snap (reconExpr, 0.5f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.0f));
    outputSm.setCurrentAndTargetValue (snap (outputExpr, 0.0f));
    bindings.prepare (varPtr, { &rateExpr, &bitsExpr, &jitterExpr, &holdExpr,
                                &reconExpr, &mixExpr, &outputExpr });
    clearRuntimeState();
}

float SignalChain::Decimate::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Decimate::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    auto target = [] (ExpressionEvaluator& expr, float fallback, float lo, float hi)
    {
        return juce::jlimit (lo, hi, snapEval (expr, fallback));
    };
    rateSm.setTargetValue (target (rateExpr, 0.5f, 0.f, 1.f));
    bitsSm.setTargetValue (target (bitsExpr, 8.f, 1.f, 16.f));
    jitterSm.setTargetValue (target (jitterExpr, 0.f, 0.f, 1.f));
    holdSm.setTargetValue (target (holdExpr, 0.f, 0.f, 1.f));
    reconSm.setTargetValue (target (reconExpr, 0.5f, 0.f, 1.f));
    mixSm.setTargetValue (target (mixExpr, 1.f, 0.f, 1.f));
    outputSm.setTargetValue (target (outputExpr, 0.f, -36.f, 12.f));

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float rate   = juce::jlimit (0.f, 1.f, rateSm.getNextValue());
        const int bits     = juce::jlimit (1, 16, (int) std::lround (bitsSm.getNextValue()));
        const float jitter = juce::jlimit (0.f, 1.f, jitterSm.getNextValue());
        const float hold   = juce::jlimit (0.f, 1.f, holdSm.getNextValue());
        const float recon  = juce::jlimit (0.f, 1.f, reconSm.getNextValue());
        const float mix    = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
        const float trimLin = juce::Decibels::decibelsToGain (
            juce::jlimit (-36.f, 12.f, outputSm.getNextValue()));

        // A sample-and-hold interval is shared by L/R: stereo stays aligned.
        // Hold extends the normal decimation period, jitter perturbs its length.
        // At both zero, the original decimation timing is unchanged.
        if (holdCounter == 0)
        {
            const int nominal = (int) juce::jlimit (1.f, 100.f,
                1.f / (1.f - rate + 1.0e-6f));
            const int base = juce::jlimit (1, 200,
                nominal + (int) std::lround (hold * 99.f));
            int duration = base;
            if (jitter > 0.f)
            {
                jitterState ^= jitterState << 13;
                jitterState ^= jitterState >> 17;
                jitterState ^= jitterState << 5;
                const float noise = (float) (jitterState & 0xffffu) / 32767.5f - 1.f;
                duration = (int) std::lround ((float) base
                    * (1.f + noise * 0.5f * jitter));
            }
            nextHoldSamples = juce::jlimit (1, 300, duration);
        }

        if (++holdCounter >= nextHoldSamples)
        {
            holdCounter = 0;
            holdL = out[0][i];
            holdR = useCh > 1 ? out[1][i] : holdL;
        }

        // Bit depth is a discrete property; use an integer shift instead of
        // a transcendental pow() on every sample.
        const float levels = (float) (1u << bits);
        const float response = 1.f - 0.98f * recon;
        for (int c = 0; c < useCh; ++c)
        {
            const float dry = out[c][i];
            const float held = (c == 0) ? holdL : holdR;
            const float quantized = std::round (held * levels) / levels;
            float& last = (c == 0) ? lastSampleL : lastSampleR;
            const float reconOut = last + response * (quantized - last);
            last = reconOut;
            out[c][i] = dry * (1.0f - mix) + reconOut * trimLin * mix;
        }
    }
}
