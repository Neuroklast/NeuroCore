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

    const int useCh = juce::jmin (nCh, 2);
    float* out[2] {};
    for (int c = 0; c < useCh; ++c)
        out[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float rate    = juce::jlimit (0.0f, 1.0f, rateSm.getNextValue());
        const float bits    = juce::jlimit (1.0f, 16.0f, bitsSm.getNextValue());
        jitterSm.skip (1);
        holdSm.skip (1);
        const float recon   = juce::jlimit (0.0f, 1.0f, reconSm.getNextValue());
        const float mix     = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());
        const float trimLin = juce::Decibels::decibelsToGain (juce::jlimit (-36.0f, 12.0f, outputSm.getNextValue()));

        const int holdN = static_cast<int> (juce::jlimit (1.0f, 100.0f, 1.0f / (1.0f - rate + 1.0e-6f)));
        if (++holdCounter >= holdN)
        {
            holdCounter = 0;
            holdL = out[0][i];
            holdR = useCh > 1 ? out[1][i] : holdL;
        }

        const int levels = static_cast<int> (juce::jlimit (1.0f, 65536.0f, std::pow (2.0f, bits)));
        for (int c = 0; c < useCh; ++c)
        {
            const float dry = out[c][i];
            const float held = (c == 0) ? holdL : holdR;
            const float quantized = std::floor (held * static_cast<float> (levels)) / static_cast<float> (levels);
            float& last = (c == 0) ? lastSampleL : lastSampleR;
            const float reconOut = quantized * (1.0f - recon) + last * recon;
            last = reconOut;
            out[c][i] = dry * (1.0f - mix) + reconOut * trimLin * mix;
        }
    }
}
