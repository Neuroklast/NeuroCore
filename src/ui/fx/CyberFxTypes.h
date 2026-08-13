#pragma once

#include <cstdint>

enum class CyberMotion : uint8_t { Full, Reduced };

enum class OverlayPhase : uint8_t
{
    Idle,
    EnterScrim,
    EnterGlitch,
    EnterReveal,
    Shown,
    ExitGlitch,
    ExitScrim,
    Closed
};

struct CyberFxState
{
    float timeSec = 0.f;
    float glitch = 0.f;
    int   glitchSeed = 0;
    float peakPulse = 0.f;
    bool  visible = true;
    CyberMotion motion = CyberMotion::Full;
};

struct CyberFxConfig
{
    float ambientHz = 24.f;
    float glitchMinGapSec = 2.5f;
    float glitchMaxGapSec = 7.5f;
    float glitchChance = 0.18f;
    float glitchMin = 0.15f;
    float glitchMax = 0.40f;
    float pulseSmooth = 0.15f;
};

inline constexpr float kEnterScrimEnd  = 0.080f;
inline constexpr float kEnterGlitchEnd = 0.220f;
inline constexpr float kEnterRevealEnd = 0.400f;
inline constexpr float kExitGlitchEnd  = 0.120f;
inline constexpr float kExitScrimEnd   = 0.280f;
inline constexpr float kScrimMax       = 0.72f;
inline constexpr int   kMaxGlitchSlices = 2;
inline constexpr float kGlitchPaintMin = 0.05f;
inline constexpr float kBootMaxSec     = 0.700f;

inline bool shouldPlayBoot (CyberMotion motion, bool alreadyShown) noexcept
{
    return motion == CyberMotion::Full && ! alreadyShown;
}
