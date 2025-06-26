#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <vector>
#include "../core/Config.h"
#include "LookupTableSmoother.h"

// Helper class that stores lookup tables for common curves.
// Tables are initialised once at plugin start.
class LookupTables
{
public:
    // Initialise all tables with the given resolution (clamped 256..4096).
    static void initialise (int size = Config::kLookupTableSize);

    // Ensure tables for additional functions are allocated if required.
    static void prepareFromScript (const juce::String& script);

    using SmoothingOptions = LookupTableSmoother::Options;
    static void setSmoothingOptions (const SmoothingOptions& opts);

    // Fast access functions using the lookup tables.
    static float fastSin  (float x) noexcept;
    static float fastCos  (float x) noexcept;
    static float fastTanh (float x) noexcept;
    static float fastExp  (float x) noexcept;
    static float fastLog  (float x) noexcept;
    static float fastPow  (float x, float exponent) noexcept;

private:
    static int tableSize;
    static std::vector<float> sinTable;
    static std::vector<float> cosTable;
    static std::vector<float> tanhTable;
    static std::vector<float> expTable;
    static std::vector<float> logTable;
    static std::unordered_map<int, std::vector<float>> powTables;
    static SmoothingOptions smoothing;
};

