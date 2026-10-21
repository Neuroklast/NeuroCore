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
}

void SignalChain::TapeStop::clearRuntimeState() noexcept
{
    for (auto& buf : delayBuf)
        buf.assign (static_cast<size_t> (sampleRate * 2.0f), 0.0f);
    writePos = 0;
    readPos = 0.0f;
    rate = 1.0f;
    active = false;
}

void SignalChain::TapeStop::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    const size_t maxDelay = static_cast<size_t> (sampleRate * 2.0f);
    for (auto& buf : delayBuf)
        buf.assign (maxDelay, 0.0f);
    writePos = 0;
    readPos = 0.0f;
    rate = 1.0f;
    active = false;
    stopSm.reset (sampleRate, Config::kSmoothingTime);
    depthSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    stopSm.setCurrentAndTargetValue (snap (stopExpr, 1.0f));
    depthSm.setCurrentAndTargetValue (snap (depthExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    bindings.prepare (varPtr, { &stopExpr, &depthExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::TapeStop::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::TapeStop::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    const int useCh = juce::jmin (nCh, 2);
    if (nS <= 0 || useCh <= 0 || delayBuf[0].empty())
        return;

    bindings.refresh();

    const size_t bufSize = delayBuf[0].size();
    float* io[2] {};
    for (int c = 0; c < useCh; ++c)
        io[c] = buffer.getWritePointer (c);

    for (int i = 0; i < nS; ++i)
    {
        const float stopSec = juce::jlimit (0.01f, 30.0f, snapEval (stopExpr, 1.0f));
        const float depth   = juce::jlimit (0.0f, 1.0f, snapEval (depthExpr, 1.0f));
        const float mix     = juce::jlimit (0.0f, 1.0f, snapEval (mixExpr, 1.0f));

        float in[2] {};
        for (int c = 0; c < useCh; ++c)
        {
            in[c] = io[c][i];
            delayBuf[(size_t) c][writePos] = in[c];
        }
        writePos = (writePos + 1) % bufSize;

        // Variable-rate read: slow down over stopSec, pitch down by depth
        if (! active)
        {
            active = true;
            rate = 1.0f;
        }

        // Ramp rate down over stopSec
        const float targetRate = 1.0f - depth;
        rate += (targetRate - rate) * (1.0f / (stopSec * sampleRate + 1.0f));
        rate = juce::jlimit (0.05f, 1.0f, rate);

        readPos += rate;
        while (readPos < 0.0f) readPos += static_cast<float> (bufSize);
        while (readPos >= static_cast<float> (bufSize)) readPos -= static_cast<float> (bufSize);

        const size_t i0 = static_cast<size_t> (readPos) % bufSize;
        const size_t i1 = (i0 + 1) % bufSize;
        const float frac = readPos - static_cast<float> (i0);

        for (int c = 0; c < useCh; ++c)
        {
            const float delayed = delayBuf[(size_t) c][i0] * (1.0f - frac)
                                + delayBuf[(size_t) c][i1] * frac;
            io[c][i] = in[c] * (1.0f - mix) + delayed * mix;
        }
    }
}
