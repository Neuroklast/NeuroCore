#include "../SignalChain.h"
#include "../../core/Config.h"
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
    delayBuf.assign (static_cast<size_t> (sampleRate * 2.0f), 0.0f);
    writePos = 0;
}

void SignalChain::ReverseDelay::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    const size_t maxDelay = static_cast<size_t> (sampleRate * 2.0f);
    delayBuf.assign (maxDelay, 0.0f);
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
    if (nS <= 0 || nCh <= 0 || delayBuf.empty())
        return;

    bindings.refresh();

    const size_t bufSize = delayBuf.size();

    for (int i = 0; i < nS; ++i)
    {
        const float timeMs = juce::jlimit (1.0f, 2000.0f, snapEval (timeExpr, 250.0f));
        const float timeSec = timeMs * 0.001f;
        const float feedback = juce::jlimit (0.0f, 0.99f, snapEval (feedbackExpr, 0.3f));
        const float mix = juce::jlimit (0.0f, 1.0f, snapEval (mixExpr, 0.5f));

        const float in = buffer.getReadPointer (0)[i];

        // Write input into buffer
        delayBuf[writePos] = in + delayBuf[writePos] * feedback;
        writePos = (writePos + 1) % bufSize;

        // Read backwards from write position at timeSec offset
        float readOffset = timeSec * sampleRate;
        float rawReadPos = static_cast<float> (writePos) - readOffset;
        while (rawReadPos < 0.0f) rawReadPos += static_cast<float> (bufSize);
        while (rawReadPos >= static_cast<float> (bufSize)) rawReadPos -= static_cast<float> (bufSize);

        const size_t i0 = static_cast<size_t> (rawReadPos) % bufSize;
        const size_t i1 = (i0 + bufSize - 1) % bufSize; // read backwards
        const float frac = rawReadPos - static_cast<float> (i0);
        const float delayed = lerp (delayBuf[i0], delayBuf[i1], frac);

        const float dryG = 1.0f - mix;
        const float wetG = mix;
        const float out = in * dryG + delayed * wetG;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            w[i] = out;
        }
    }
}
