#pragma once

#include "CyberFxTypes.h"

class CyberSequence
{
public:
    void playEnter();
    void playExit();
    void skipToEnd();
    void tick (float dtSec);

    OverlayPhase getPhase() const noexcept { return phase; }
    float timeSec() const noexcept { return t; }
    float scrimAlpha() const noexcept;
    float contentAlpha() const noexcept;
    float wipeY01() const noexcept;
    float sliceAmount() const noexcept;
    float timeline01() const noexcept;
    float clipProgress() const noexcept;
    bool  isLoaderVisible() const noexcept;
    bool  isBusy() const noexcept;
    bool  consumeFinished();

private:
    enum class Mode : uint8_t { None, Enter, Exit };

    static float smoothstep (float t) noexcept;

    void syncPhase();

    Mode mode { Mode::None };
    OverlayPhase phase { OverlayPhase::Idle };
    float t { 0.f };
    bool finishedPending { false };
};
