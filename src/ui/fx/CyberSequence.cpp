#include "CyberSequence.h"
#include <JuceHeader.h>
#include <cmath>

void CyberSequence::playEnter()
{
    mode = Mode::Enter;
    t = 0.f;
    finishedPending = false;
    phase = OverlayPhase::EnterScrim;
}

void CyberSequence::playExit()
{
    if (phase == OverlayPhase::Idle || phase == OverlayPhase::Closed)
    {
        mode = Mode::None;
        phase = OverlayPhase::Closed;
        finishedPending = true;
        t = kExitScrimEnd;
        return;
    }

    mode = Mode::Exit;
    t = 0.f;
    finishedPending = false;
    phase = OverlayPhase::ExitGlitch;
}

void CyberSequence::skipToEnd()
{
    if (mode == Mode::Exit
        || phase == OverlayPhase::ExitGlitch
        || phase == OverlayPhase::ExitScrim)
    {
        mode = Mode::None;
        phase = OverlayPhase::Closed;
        t = kExitScrimEnd;
        finishedPending = true;
        return;
    }

    mode = Mode::Enter;
    phase = OverlayPhase::Shown;
    t = kEnterRevealEnd;
    finishedPending = false;
}

void CyberSequence::tick (float dtSec)
{
    if (mode == Mode::None)
        return;

    t += juce::jmax (0.f, dtSec);
    syncPhase();
}

void CyberSequence::syncPhase()
{
    if (mode == Mode::Enter)
    {
        if (t >= kEnterRevealEnd)
            phase = OverlayPhase::Shown;
        else if (t >= kEnterGlitchEnd)
            phase = OverlayPhase::EnterReveal;
        else if (t >= kEnterScrimEnd)
            phase = OverlayPhase::EnterGlitch;
        else
            phase = OverlayPhase::EnterScrim;
        return;
    }

    if (mode == Mode::Exit)
    {
        if (t >= kExitScrimEnd)
        {
            phase = OverlayPhase::Closed;
            mode = Mode::None;
            finishedPending = true;
        }
        else if (t >= kExitGlitchEnd)
            phase = OverlayPhase::ExitScrim;
        else
            phase = OverlayPhase::ExitGlitch;
    }
}

float CyberSequence::smoothstep (float x) noexcept
{
    x = juce::jlimit (0.f, 1.f, x);
    return x * x * (3.f - 2.f * x);
}

float CyberSequence::scrimAlpha() const noexcept
{
    switch (phase)
    {
        case OverlayPhase::EnterScrim:
            return kScrimMax * smoothstep (t / kEnterScrimEnd);
        case OverlayPhase::EnterGlitch:
        case OverlayPhase::EnterReveal:
        case OverlayPhase::Shown:
        case OverlayPhase::ExitGlitch:
            return kScrimMax;
        case OverlayPhase::ExitScrim:
            return kScrimMax * (1.f - smoothstep ((t - kExitGlitchEnd)
                                                  / (kExitScrimEnd - kExitGlitchEnd)));
        case OverlayPhase::Idle:
        case OverlayPhase::Closed:
        default:
            return 0.f;
    }
}

float CyberSequence::contentAlpha() const noexcept
{
    if (phase == OverlayPhase::Shown)
        return 1.f;
    if (mode == Mode::Enter)
        return t >= kEnterRevealEnd ? 1.f : 0.f;
    if (mode == Mode::Exit)
        return t < 0.07f ? 1.f - t / 0.07f : 0.f;
    return 0.f;
}

float CyberSequence::wipeY01() const noexcept
{
    if (phase == OverlayPhase::EnterGlitch)
        return juce::jlimit (0.f, 1.f, (t - kEnterScrimEnd) / (kEnterGlitchEnd - kEnterScrimEnd));
    if (phase == OverlayPhase::ExitGlitch)
        return juce::jlimit (0.f, 1.f, t / kExitGlitchEnd);
    return 0.f;
}

float CyberSequence::sliceAmount() const noexcept
{
    if (mode == Mode::Enter)
        return t < 0.11f ? 1.f - t / 0.11f : 0.f;
    if (mode == Mode::Exit)
        return t < 0.10f ? 1.f - t / 0.10f : 0.f;
    return 0.f;
}

float CyberSequence::timeline01() const noexcept
{
    if (mode == Mode::Enter)
        return juce::jlimit (0.f, 1.f, t / kEnterRevealEnd);
    if (mode == Mode::Exit)
        return juce::jlimit (0.f, 1.f, t / kExitScrimEnd);
    if (phase == OverlayPhase::Shown)
        return 1.f;
    return 0.f;
}

float CyberSequence::clipProgress() const noexcept
{
    if (phase == OverlayPhase::Shown)
        return 1.f;
    if (mode == Mode::Enter)
        return smoothstep (t / 0.36f);
    if (mode == Mode::Exit)
    {
        if (t < kExitGlitchEnd)
            return 1.f;
        return 1.f - smoothstep ((t - kExitGlitchEnd) / (kExitScrimEnd - kExitGlitchEnd));
    }
    return 0.f;
}

bool CyberSequence::isLoaderVisible() const noexcept
{
    if (clipProgress() < 0.12f)
        return false;
    if (mode == Mode::Enter)
        return t < kEnterRevealEnd;
    if (mode == Mode::Exit)
        return t < kExitGlitchEnd + 0.06f;
    return false;
}

bool CyberSequence::isBusy() const noexcept
{
    return phase == OverlayPhase::EnterScrim
        || phase == OverlayPhase::EnterGlitch
        || phase == OverlayPhase::EnterReveal
        || phase == OverlayPhase::ExitGlitch
        || phase == OverlayPhase::ExitScrim;
}

bool CyberSequence::consumeFinished()
{
    if (! finishedPending)
        return false;
    finishedPending = false;
    return true;
}
