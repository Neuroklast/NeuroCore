#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
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
    // SIMD versions
    static juce::dsp::SIMDRegister<float> fastSinSimd (const juce::dsp::SIMDRegister<float>& x) noexcept;
    static juce::dsp::SIMDRegister<float> fastCosSimd (const juce::dsp::SIMDRegister<float>& x) noexcept;
    static juce::dsp::SIMDRegister<float> fastTanhSimd(const juce::dsp::SIMDRegister<float>& x) noexcept;
    static juce::dsp::SIMDRegister<float> fastExpSimd (const juce::dsp::SIMDRegister<float>& x) noexcept;
    static juce::dsp::SIMDRegister<float> fastLogSimd (const juce::dsp::SIMDRegister<float>& x) noexcept;
    static juce::dsp::SIMDRegister<float> fastPowSimd (const juce::dsp::SIMDRegister<float>& x, float exponent) noexcept;

    /** 64-byte-aligned saturation / trig tables. Size is the logical LUT length. */
    static const float* sinData() noexcept;
    static const float* tanhData() noexcept;
    static int size() noexcept;

private:
    static int tableSize;
    static std::vector<float> sinStore, cosStore, tanhStore, expStore, logStore;
    static float* sinTable;
    static float* cosTable;
    static float* tanhTable;
    static float* expTable;
    static float* logTable;
    static std::unordered_map<int, std::pair<std::vector<float>, float*>> powTables;
    static SmoothingOptions smoothing;
};

