#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <cmath>
#include <vector>

using namespace dsl;

namespace
{
    float snapEval (ExpressionEvaluator& e, float fallback) noexcept
    {
        const float v = e.evaluateLive (0.0f);
        return std::isfinite (v) ? v : fallback;
    }

    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
}

void SignalChain::ReverseDelay::clearRuntimeState() noexcept
{
    for (int c = 0; c < channels; ++c)
        delayBuf[(size_t) c].assign (static_cast<size_t> (sampleRate * 2.0f), 0.0f);
    writePos = 0;
}

void SignalChain::ReverseDelay::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = juce::jlimit (1, 2, static_cast<int> (spec.numChannels));
    const size_t maxDelay = static_cast<size_t> (sampleRate * 2.0f);
    for (int c = 0; c < channels; ++c)
        delayBuf[(size_t) c].assign (maxDelay, 0.0f);
    writePos = 0;
    timeSm.reset (sampleRate, Config::kSmoothingTime);
    feedbackSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    timeSm.setCurrentAndTargetValue (snap (timeExpr, 0.5f));
    feedbackSm.setCurrentAndTargetValue (snap (feedbackExpr, 0.3f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.5f));
    bindings.prepare (varPtr, { &timeExpr, &feedbackExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::ReverseDelay::tailSeconds() const noexcept
{
    float ms = timeExpr.evaluateLive (0.f);
    if (! std::isfinite (ms)) ms = 250.f;
    float fb = feedbackExpr.evaluateLive (0.f);
    if (! std::isfinite (fb)) fb = 0.35f;
    const float delaySec = juce::jlimit (0.001f, kMaxDelaySec, ms * 0.001f);
    return DSPUtils::feedbackTailSeconds (delaySec, fb);
}

float SignalChain::ReverseDelay::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::ReverseDelay::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    const int useCh = juce::jmin (nCh, channels);
    if (nS <= 0 || useCh <= 0 || delayBuf[0].empty())
        return;

    bindings.refresh();

    timeSm.setTargetValue (snapEval (timeExpr, 250.0f));
    feedbackSm.setTargetValue (snapEval (feedbackExpr, 0.3f));
    mixSm.setTargetValue (snapEval (mixExpr, 0.5f));

    const size_t bufSize = delayBuf[0].size();
    float* io[2] {};
    for (int c = 0; c < useCh; ++c)
        io[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float timeMs = juce::jlimit (1.0f, 2000.0f, timeSm.getNextValue());
        const float timeSec = timeMs * 0.001f;
        const float feedback = juce::jlimit (0.0f, 0.99f, feedbackSm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        float in[2] {};
        for (int c = 0; c < useCh; ++c)
        {
            in[c] = io[c][i];
            delayBuf[(size_t) c][writePos] = in[c] + delayBuf[(size_t) c][writePos] * feedback;
        }
        writePos = (writePos + 1) % bufSize;

        // Read backwards from write position at timeSec offset
        const float readOffset = timeSec * sampleRate;
        float rawReadPos = static_cast<float> (writePos) - readOffset;
        while (rawReadPos < 0.0f) rawReadPos += static_cast<float> (bufSize);
        while (rawReadPos >= static_cast<float> (bufSize)) rawReadPos -= static_cast<float> (bufSize);

        const size_t i0 = static_cast<size_t> (rawReadPos) % bufSize;
        const size_t i1 = (i0 + bufSize - 1) % bufSize; // read backwards
        const float frac = rawReadPos - static_cast<float> (i0);

        const float dryG = 1.0f - mix;
        const float wetG = mix;

        for (int c = 0; c < useCh; ++c)
        {
            const float delayed = lerp (delayBuf[(size_t) c][i0], delayBuf[(size_t) c][i1], frac);
            io[c][i] = in[c] * dryG + delayed * wetG;
        }
    }
}
