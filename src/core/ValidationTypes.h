#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file ValidationTypes.h
    @brief Shared types used across the validation subsystem.
*/

#include <JuceHeader.h>

/** Progress info structure passed to the stability-validation callback. */
struct ValidationProgressInfo
{
    float       progress { 0.0f };   ///< Progress in range [0, 1]
    juce::String message;             ///< Description of current task
    int         nanCount { 0 };       ///< Number of NaN samples seen
    int         infCount { 0 };       ///< Number of Inf samples seen
};
