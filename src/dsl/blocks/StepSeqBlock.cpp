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

    uint32_t hashStep (uint32_t seed, long long step) noexcept
    {
        uint32_t x = seed + 0x9e3779b9u + (uint32_t) (step * 0x85ebca6bu);
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    float hash01 (uint32_t seed, long long step) noexcept
    {
        return (float) (hashStep (seed, step) >> 8) / 16777216.0f;
    }
}

void SignalChain::StepSeq::clearRuntimeState() noexcept
{
    phase = 0.0;
    stepIndex = 0;
    absoluteStep = 0;
    gateStep = -1;
    gateStepOn = true;
    glideValue = 0.0f;
    glideCoefficient = 1.0f;
    glideCoefficientStep = 0;
    pendingReset = true;
    liveStep.store (-1, std::memory_order_relaxed);
    if (liveStepSlot != nullptr)
        liveStepSlot->store (-1, std::memory_order_relaxed);
}

void SignalChain::StepSeq::setHost (double bpm, double ppq, bool playing, bool reset) noexcept
{
    if (bpm > 0.0)
        hostBpm = bpm;
    hostPpq = ppq;
    hostPlaying = playing;
    if (reset && resetOnStart)
        pendingReset = true;
}

float SignalChain::StepSeq::stepValue (int index) const noexcept
{
    if (index < 0 || index >= stepCount || index >= kMaxSteps)
        return 0.0f;
    if (muted[(size_t) index])
        return 0.0f;
    return juce::jlimit (-1.0f, 1.0f, values[(size_t) index]);
}

int SignalChain::StepSeq::mapStep (long long absStep) const noexcept
{
    const int n = juce::jlimit (1, kMaxSteps, stepCount);
    if (n <= 1)
        return 0;

    switch (direction)
    {
    case Direction::Reverse:
    {
        auto m = absStep % n;
        if (m < 0) m += n;
        return n - 1 - (int) m;
    }
    case Direction::PingPong:
    {
        const long long period = 2LL * n - 2;
        auto m = absStep % period;
        if (m < 0) m += period;
        return m < n ? (int) m : (int) (period - m);
    }
    case Direction::Random:
        return (int) (hashStep (seed, absStep) % (uint32_t) n);
    case Direction::Forward:
    default:
    {
        auto m = absStep % n;
        if (m < 0) m += n;
        return (int) m;
    }
    }
}

float SignalChain::StepSeq::probabilityFor (long long absStep) const noexcept
{
    return hash01 (seed, absStep);
}

void SignalChain::StepSeq::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    rateSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    gateSm.reset (sampleRate, Config::kSmoothingTime);
    glideSm.reset (sampleRate, Config::kSmoothingTime);
    swingSm.reset (sampleRate, Config::kSmoothingTime);
    probabilitySm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    rateSm.setCurrentAndTargetValue (snap (rateExpr, 1.0f));
    mixSm.setCurrentAndTargetValue (snap (mixExpr, 1.0f));
    gateSm.setCurrentAndTargetValue (snap (gateExpr, 1.0f));
    glideSm.setCurrentAndTargetValue (snap (glideExpr, 0.0f));
    swingSm.setCurrentAndTargetValue (snap (swingExpr, 0.0f));
    probabilitySm.setCurrentAndTargetValue (snap (probabilityExpr, 1.0f));
    bindings.prepare (varPtr, { &rateExpr, &mixExpr, &gateExpr,
                                &glideExpr, &swingExpr, &probabilityExpr,
                                &stepExpr[0], &stepExpr[1], &stepExpr[2], &stepExpr[3] });
    reserveControlLane (spec);
    clearRuntimeState();
}

float SignalChain::StepSeq::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::StepSeq::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nS = buffer.getNumSamples();
    if (nS <= 0)
        return;

    const int laneN = juce::jmin (nS, (int) modLane.size());
    if (laneN <= 0)
        return;

    bindings.refresh();

    rateSm.setTargetValue (snapEval (rateExpr, 1.0f));
    mixSm.setTargetValue (snapEval (mixExpr, 1.0f));
    gateSm.setTargetValue (snapEval (gateExpr, 1.0f));
    glideSm.setTargetValue (snapEval (glideExpr, 0.0f));
    swingSm.setTargetValue (snapEval (swingExpr, 0.0f));
    probabilitySm.setTargetValue (snapEval (probabilityExpr, 1.0f));

    if (legacyStepsFromArgs)
        for (int i = 0; i < juce::jmin (4, stepCount); ++i)
            values[(size_t) i] = juce::jlimit (-1.0f, 1.0f, snapEval (stepExpr[i], 0.0f));

    if (pendingReset)
    {
        phase = 0.0;
        absoluteStep = 0;
        stepIndex = 0;
        gateStep = -1;
        glideValue = 0.0f;
        glideCoefficient = 1.0f;
        glideCoefficientStep = 0;
        pendingReset = false;
    }

    const double invSr = 1.0 / (double) sampleRate;
    const double bpm = hostBpm > 0.0 ? hostBpm : (double) Config::kDefaultTempo;
    const double stepBeats = juce::jmax (1.0 / 64.0, (double) syncWhole * 4.0);
    const double pairBeats = stepBeats * 2.0;
    int lastStep = -1;

    for (int i = 0; i < laneN; ++i)
    {
        const float gate = juce::jlimit (0.0f, 1.0f, gateSm.getNextValue());
        const float glide = juce::jlimit (0.0f, 1.0f, glideSm.getNextValue());
        const float swing = juce::jlimit (0.0f, 0.95f, swingSm.getNextValue());
        const float probability = juce::jlimit (0.0f, 1.0f, probabilitySm.getNextValue());
        const float mix = juce::jlimit (0.0f, 1.0f, mixSm.getNextValue());

        float stepPhase = 0.0f;
        if (syncEnabled)
        {
            if (! hostPlaying)
            {
                modLane[(size_t) i] = 0.0f;
                continue;
            }

            const double ppq = hostPpq + (double) i * (bpm / 60.0) * invSr;
            const double pairIndex = std::floor (ppq / pairBeats);
            // The quotient is already known. Recover its fractional beat
            // position by subtraction instead of calling std::fmod() for
            // every sample of every sequencer at the OS sample rate.
            double pairPos = ppq - pairIndex * pairBeats;
            if (pairPos < 0.0)
                pairPos += pairBeats;
            else if (pairPos >= pairBeats)
                pairPos -= pairBeats;

            const double first = stepBeats * (1.0 + swing);
            const double second = juce::jmax (1.0e-6, pairBeats - first);
            long long absStep;
            if (pairPos < first)
            {
                absStep = (long long) pairIndex * 2;
                stepPhase = (float) (pairPos / juce::jmax (1.0e-6, first));
            }
            else
            {
                absStep = (long long) pairIndex * 2 + 1;
                stepPhase = (float) ((pairPos - first) / second);
            }

            absoluteStep = absStep;
            stepIndex = mapStep (absoluteStep);
        }
        else
        {
            const float rate = juce::jlimit (0.0f, 100.0f, rateSm.getNextValue());
            const double baseDur = 1.0 / (double) juce::jmax (1.0e-5f, rate);
            const bool odd = (absoluteStep & 1LL) != 0;
            const double factor = juce::jmax (0.05, odd ? (1.0 - swing) : (1.0 + swing));
            phase += invSr / (baseDur * factor);
            while (phase >= 1.0)
            {
                phase -= 1.0;
                ++absoluteStep;
            }
            stepIndex = mapStep (absoluteStep);
            stepPhase = (float) phase;
        }

        lastStep = stepIndex;

        if (absoluteStep != gateStep)
        {
            gateStep = absoluteStep;
            gateStepOn = probability >= 1.0f
                      || (probability > 0.0f && probabilityFor (absoluteStep) < probability);
        }

        const float target = (gateStepOn && stepPhase < gate) ? stepValue (stepIndex) : 0.0f;
        if (glide <= 1.0e-4f)
        {
            glideValue = target;
            glideCoefficientStep = 0;
        }
        else
        {
            // Update the expensive exponential at a 16-sample control
            // cadence. The envelope state still advances at audio rate.
            // A step from glide=0 always refreshes immediately.
            if (glideCoefficientStep == 0)
            {
                const float tc = juce::jmax (0.001f, glide * 0.5f);
                glideCoefficient = -std::expm1 (-1.0f / (tc * sampleRate));
            }
            glideCoefficientStep = (glideCoefficientStep + 1) & 15;
            glideValue += (target - glideValue) * glideCoefficient;
        }

        modLane[(size_t) i] = glideValue * mix;
    }

    liveStep.store (lastStep, std::memory_order_relaxed);
    if (liveStepSlot != nullptr)
        liveStepSlot->store (lastStep, std::memory_order_relaxed);

    if (syncEnabled && hostPlaying)
        hostPpq += (double) nS * (bpm / 60.0) * invSr;
}
