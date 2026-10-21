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

void SignalChain::Adsr::clearRuntimeState() noexcept
{
    state = State::Idle;
    env = 0.0f;
    gatePrev = 0.0f;
}

void SignalChain::Adsr::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    attackSm.reset (sampleRate, Config::kSmoothingTime);
    decaySm.reset (sampleRate, Config::kSmoothingTime);
    sustainSm.reset (sampleRate, Config::kSmoothingTime);
    releaseSm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    attackSm.setCurrentAndTargetValue (snap (attackExpr, 0.1f));
    decaySm.setCurrentAndTargetValue (snap (decayExpr, 0.1f));
    sustainSm.setCurrentAndTargetValue (snap (sustainExpr, 0.7f));
    releaseSm.setCurrentAndTargetValue (snap (releaseExpr, 0.1f));
    bindings.prepare (varPtr, { &attackExpr, &decayExpr, &sustainExpr, &releaseExpr });
    clearRuntimeState();
}

float SignalChain::Adsr::process (int ch, float x)
{
    juce::ignoreUnused (ch);
    juce::ignoreUnused (x);
    return x;
}

void SignalChain::Adsr::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    bindings.refresh();

    attackSm.setTargetValue (snapEval (attackExpr, 0.1f));
    decaySm.setTargetValue (snapEval (decayExpr, 0.1f));
    sustainSm.setTargetValue (snapEval (sustainExpr, 0.7f));
    releaseSm.setTargetValue (snapEval (releaseExpr, 0.1f));

    const float invSr = 1.0f / sampleRate;

    for (int i = 0; i < nS; ++i)
    {
        const float gate = (nCh > 0) ? buffer.getReadPointer (0)[i] : 0.0f;
        const bool gateOn = gate > 0.5f;

        const float atk = juce::jlimit (0.001f, 10.0f, attackSm.getNextValue());
        const float dec = juce::jlimit (0.001f, 10.0f, decaySm.getNextValue());
        const float sus = juce::jlimit (0.0f, 1.0f, sustainSm.getNextValue());
        const float rel = juce::jlimit (0.001f, 10.0f, releaseSm.getNextValue());

        // Gate edge detection
        if (gateOn && ! gatePrev)
        {
            state = State::Attack;
            env = 0.0f;
        }
        else if (! gateOn && gatePrev && state != State::Idle)
        {
            state = State::Release;
        }

        switch (state)
        {
        case State::Attack:
            env += (1.0f - env) * (invSr / atk);
            if (env >= 1.0f) { env = 1.0f; state = State::Decay; }
            break;
        case State::Decay:
            env += (sus - env) * (invSr / dec);
            if (env <= sus) { env = sus; state = State::Sustain; }
            break;
        case State::Sustain:
            env = sus;
            break;
        case State::Release:
            env -= env * (invSr / rel);
            if (env <= 0.001f) { env = 0.0f; state = State::Idle; }
            break;
        case State::Idle:
            env = 0.0f;
            break;
        }
        gatePrev = gateOn;

        for (int c = 0; c < nCh; ++c)
        {
            float* w = buffer.getWritePointer (c);
            w[i] = env;
        }
    }
}
