#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

namespace EffectParameters
{
    inline constexpr const char* inputGain    = "inputGain";
    inline constexpr const char* paramA       = "a";
    inline constexpr const char* paramB       = "b";
    inline constexpr const char* paramC       = "c";
    inline constexpr const char* paramD       = "d";
    inline constexpr const char* paramE       = "e";
    inline constexpr const char* paramF       = "f";
    /// Indexed access for a..f (must match Config::kNumUserParams).
    inline constexpr const char* userParams[6] = {
        "a", "b", "c", "d", "e", "f"
    };
    inline constexpr const char* polisherMode = "polisherMode";
    inline constexpr const char* dryWet       = "dryWet";
    inline constexpr const char* outputGain   = "outputGain";
    inline constexpr const char* useInputLeft  = "useInputLeft";
    inline constexpr const char* useInputRight = "useInputRight";
    inline constexpr const char* oversampling  = "oversampling";
    /// 0 = off (unity), 1 = full mild loudness match. No UI required; host-automatable.
    inline constexpr const char* autoGain      = "autoGain";
}
