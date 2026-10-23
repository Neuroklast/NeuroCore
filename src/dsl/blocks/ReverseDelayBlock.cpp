#include "../SignalChain.h"
#include "../../core/Config.h"
#include "../../dsp/DSPUtils.h"
#include <algorithm>
#include <cmath>

using namespace dsl;

namespace
{
    float snapEval (ExpressionEvaluator& e, float fallback) noexcept
    {
        const float v = e.evaluateLive (0.f);
        return std::isfinite (v) ? v : fallback;
    }

    float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
}

void SignalChain::ReverseDelay::clearRuntimeState() noexcept
{
    for (int ch = 0; ch < channels; ++ch)
        std::fill (delayBuf[(size_t) ch].begin(), delayBuf[(size_t) ch].end(), 0.f);
    writePos = captureStart = previousStart = 0;
    capturePos = captureSamples = previousSamples = 0;
    feedbackWet[0] = feedbackWet[1] = 0.f;
}

void SignalChain::ReverseDelay::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = juce::jlimit (1, 2, (int) spec.numChannels);

    // Two entire maximum-length segments must coexist. Allocations are made
    // only during prepare, never while audio is processing.
    const size_t maxWindow = (size_t) std::ceil (sampleRate * kMaxDelaySec) + 2u;
    for (int ch = 0; ch < channels; ++ch)
        delayBuf[(size_t) ch].assign (2u * maxWindow, 0.f);

    timeSm.reset (sampleRate, Config::kSmoothingTime);
    feedbackSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    auto snap = [] (ExpressionEvaluator& e, float fallback)
    {
        const float v = e.evaluate (0.f);
        return std::isfinite (v) ? v : fallback;
    };
    timeSm.setCurrentAndTargetValue (snap (timeExpr, 250.f));
    feedbackSm.setCurrentAndTargetValue (snap (feedbackExpr, 0.3f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 0.5f));
    bindings.prepare (varPtr, { &timeExpr, &feedbackExpr, &mixExpr });
    clearRuntimeState();
}

float SignalChain::ReverseDelay::tailSeconds() const noexcept
{
    const float ms = snapEval (const_cast<ExpressionEvaluator&> (timeExpr), 250.f);
    const float fb = snapEval (const_cast<ExpressionEvaluator&> (feedbackExpr), 0.3f);
    const float window = juce::jlimit (0.001f, kMaxDelaySec, ms * 0.001f);
    // An event early in a recorded segment is not emitted until near the end
    // of the following segment. Account for both windows plus feedback.
    return juce::jlimit (0.002f, 12.f, window
                        + DSPUtils::feedbackTailSeconds (window, fb));
}

float SignalChain::ReverseDelay::process (int, float x) { return x; }

void SignalChain::ReverseDelay::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nS = buffer.getNumSamples();
    const int useCh = juce::jmin (buffer.getNumChannels(), channels);
    if (nS <= 0 || useCh <= 0 || delayBuf[0].empty())
        return;

    bindings.refresh();
    timeSm.setTargetValue (juce::jlimit (1.f, 2000.f, snapEval (timeExpr, 250.f)));
    feedbackSm.setTargetValue (juce::jlimit (0.f, 0.99f, snapEval (feedbackExpr, 0.3f)));
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, snapEval (mixExpr, 0.5f)));

    const size_t ringN = delayBuf[0].size();
    const int maxSamples = (int) (ringN / 2u - 2u);
    float* io[2] {};
    for (int ch = 0; ch < useCh; ++ch)
        io[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < nS; ++i)
    {
        const float ms = timeSm.getNextValue();
        const float feedback = juce::jlimit (0.f, 0.99f, feedbackSm.getNextValue());
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());

        // A changed delay time takes effect at a window boundary; changing
        // the reader length mid-window would introduce discontinuities.
        if (captureSamples == 0)
            captureSamples = juce::jlimit (2, maxSamples,
                (int) std::lround ((double) sampleRate * (double) ms * 0.001));

        float original[2] {};
        for (int ch = 0; ch < useCh; ++ch)
        {
            original[ch] = io[ch][i];
            const float in = original[ch] + feedback * feedbackWet[ch];
            delayBuf[(size_t) ch][writePos] = std::isfinite (in) ? in : 0.f;
        }

        // Read the *completed* previous segment from end to beginning.
        // Scaling its position allows time automation between whole windows.
        // Current capture and previous playback never overlap in the ring.
        float wet[2] {};
        if (previousSamples > 1)
        {
            const float reversePos = (float) (previousSamples - 1)
                * (1.f - (float) capturePos / (float) captureSamples);
            const int j0 = juce::jlimit (0, previousSamples - 1, (int) reversePos);
            const int j1 = juce::jmin (previousSamples - 1, j0 + 1);
            const float fraction = reversePos - (float) j0;
            const size_t idx0 = (previousStart + (size_t) j0) % ringN;
            const size_t idx1 = (previousStart + (size_t) j1) % ringN;

            // Short window fade on both ends prevents abrupt slice-boundary
            // clicks. It is not a granular timestretcher or an overlap-add reverb.
            const int fadeSamples = juce::jmax (1, juce::jmin (
                (int) (sampleRate * 0.004f),
                juce::jmin (captureSamples, previousSamples) / 16));
            const float fadeIn = juce::jmin (1.f,
                (float) (capturePos + 1) / (float) fadeSamples);
            const float fadeOut = juce::jmin (1.f,
                (float) (captureSamples - capturePos) / (float) fadeSamples);
            const float fade = juce::jmin (fadeIn, fadeOut);

            for (int ch = 0; ch < useCh; ++ch)
                wet[ch] = lerp (delayBuf[(size_t) ch][idx0],
                                delayBuf[(size_t) ch][idx1], fraction) * fade;
        }

        for (int ch = 0; ch < useCh; ++ch)
        {
            feedbackWet[ch] = std::isfinite (wet[ch]) ? wet[ch] : 0.f;
            io[ch][i] = original[ch] * (1.f - mix) + feedbackWet[ch] * mix;
        }

        writePos = (writePos + 1) % ringN;
        if (++capturePos >= captureSamples)
        {
            previousStart = captureStart;
            previousSamples = captureSamples;
            captureStart = writePos;
            capturePos = 0;
            captureSamples = 0;
        }
    }
}
