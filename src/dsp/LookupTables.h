#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <vector>
#include "../core/Config.h"

// Helper class that stores lookup tables for common curves.
// Tables are initialised once at plugin start.
class LookupTables
{
public:
    // Initialise all tables with the given resolution (clamped 256..4096).
    static void initialise (int size = Config::kLookupTableSize);

    // Fast access functions using the lookup tables.
    static float fastSin  (float x) noexcept;
    static float fastCos  (float x) noexcept;
    static float fastTanh (float x) noexcept;

private:
    static int tableSize;
    static std::vector<float> sinTable;
    static std::vector<float> cosTable;
    static std::vector<float> tanhTable;
};

