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
    phase = 0.0f;
    phaseStart = 0.0f;
}

void SignalChain::Adsr::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    channels = static_cast<int> (spec.numChannels);
    attackSm.reset (sampleRate, Config::kSmoothingTime);
    decaySm.reset (sampleRate, Config::kSmoothingTime);
    sustainSm.reset (sampleRate, Config::kSmoothingTime);
    releaseSm.reset (sampleRate, Config::kSmoothingTime);
    velocitySm.reset (sampleRate, Config::kSmoothingTime);

    auto snap = [] (ExpressionEvaluator& e, float fallback) -> float
    {
        const float v = e.evaluate (0.0f);
        return std::isfinite (v) ? v : fallback;
    };
    attackSm.setCurrentAndTargetValue (snap (attackExpr, 0.1f));
    decaySm.setCurrentAndTargetValue (snap (decayExpr, 0.1f));
    sustainSm.setCurrentAndTargetValue (snap (sustainExpr, 0.7f));
    releaseSm.setCurrentAndTargetValue (snap (releaseExpr, 0.1f));
    velocitySm.setCurrentAndTargetValue (snap (velocityExpr, 0.0f));
    bindings.prepare (varPtr, { &attackExpr, &decayExpr, &sustainExpr,
                                &releaseExpr, &velocityExpr });
    if (varPtr != nullptr)
    {
        midiGatePtr = &(*varPtr)["midi_gate"];
        midiVelPtr = &(*varPtr)["midi_vel"];
    }
    reserveControlLane (spec);
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

    const int laneN = juce::jmin (nS, (int) modLane.size());
    if (laneN <= 0)
        return;

    bindings.refresh();

    attackSm.setTargetValue (snapEval (attackExpr, 0.1f));
    decaySm.setTargetValue (snapEval (decayExpr, 0.1f));
    sustainSm.setTargetValue (snapEval (sustainExpr, 0.7f));
    releaseSm.setTargetValue (snapEval (releaseExpr, 0.1f));
    velocitySm.setTargetValue (snapEval (velocityExpr, 0.0f));

    const float invSr = 1.0f / sampleRate;
    const float* gateIn = buffer.getReadPointer (0);

    for (int i = 0; i < laneN; ++i)
    {
        const bool audioGate = gateIn[i] > 0.5f;
        const bool midiGate = midiGatePtr != nullptr && *midiGatePtr > 0.5f;
        bool gateOn = false;
        switch (gateSource)
        {
        case GateSource::Midi:   gateOn = midiGate; break;
        case GateSource::Either: gateOn = audioGate || midiGate; break;
        case GateSource::Audio:
        default:                 gateOn = audioGate; break;
        }

        const float atk = juce::jlimit (0.001f, 10.0f, attackSm.getNextValue());
        const float dec = juce::jlimit (0.001f, 10.0f, decaySm.getNextValue());
        const float sus = juce::jlimit (0.0f, 1.0f, sustainSm.getNextValue());
        const float rel = juce::jlimit (0.001f, 10.0f, releaseSm.getNextValue());
        const float vel = juce::jlimit (0.0f, 1.0f, velocitySm.getNextValue());

        if (gateOn && ! gatePrev)
        {
            if (retrigger || state == State::Idle)
            {
                state = State::Attack;
                phase = 0.0f;
                phaseStart = 0.0f;
            }
        }
        else if (! gateOn && gatePrev && state != State::Idle)
        {
            state = State::Release;
            phase = 0.0f;
            phaseStart = env;
        }

        switch (state)
        {
        case State::Attack:
            phase += invSr / atk;
            if (phase >= 1.0f)
            {
                env = 1.0f;
                state = State::Decay;
                phase = 0.0f;
                phaseStart = 1.0f;
            }
            else
            {
                env = phaseStart + (1.0f - phaseStart) * phase;
            }
            break;

        case State::Decay:
            phase += invSr / dec;
            if (phase >= 1.0f)
            {
                env = sus;
                state = State::Sustain;
                phase = 0.0f;
            }
            else
            {
                env = phaseStart + (sus - phaseStart) * phase;
            }
            break;

        case State::Sustain:
            env = sus;
            break;

        case State::Release:
            phase += invSr / rel;
            if (phase >= 1.0f)
            {
                env = 0.0f;
                state = State::Idle;
                phase = 0.0f;
            }
            else
            {
                env = phaseStart * (1.0f - phase);
            }
            break;

        case State::Idle:
        default:
            env = 0.0f;
            break;
        }

        gatePrev = gateOn;

        const float midiVelocity = midiVelPtr != nullptr && *midiVelPtr > 0.0f
                                       ? juce::jlimit (0.0f, 1.0f, *midiVelPtr)
                                       : 1.0f;
        const float shaped = env * (1.0f - vel + vel * midiVelocity);
        modLane[(size_t) i] = juce::jlimit (0.0f, 1.0f, shaped);
    }
}
