#include <JuceHeader.h>
#include "LookupTables.h"
#include <unordered_map>
#include <cmath>

int LookupTables::tableSize = 0;
std::vector<float> LookupTables::sinTable;
std::vector<float> LookupTables::cosTable;
std::vector<float> LookupTables::tanhTable;
std::vector<float> LookupTables::expTable;
std::vector<float> LookupTables::logTable;
std::unordered_map<int, std::vector<float>> LookupTables::powTables;

void LookupTables::initialise (int size)
{
    size = juce::jlimit (256, 4096, size);
    if (tableSize == size)
        return;

    tableSize = size;
    sinTable.resize (size);
    cosTable.resize (size);
    tanhTable.resize (size);

    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < size; ++i)
    {
        const float phase = (static_cast<float> (i) / (size - 1)) * twoPi;
        sinTable[i] = std::sin (phase);
        cosTable[i] = std::cos (phase);
    }

    const float range = 4.0f; // tanh lookup covers [-4,4]
    for (int i = 0; i < size; ++i)
    {
        const float x = (static_cast<float> (i) / (size - 1)) * (2.0f * range) - range;
        tanhTable[i] = std::tanh (x);
    }

    expTable.clear();
    logTable.clear();
    powTables.clear();
}

static inline float interp (const std::vector<float>& table, float pos)
{
    const int size = static_cast<int> (table.size());
    const int idx = static_cast<int> (pos);
    const int next = (idx + 1) % size;
    const float frac = pos - static_cast<float> (idx);
    return table[idx] + frac * (table[next] - table[idx]);
}

float LookupTables::fastSin (float x) noexcept
{
    if (tableSize == 0)
        return std::sin (x);

    const float twoPi = juce::MathConstants<float>::twoPi;
    x = std::fmod (x, twoPi);
    if (x < 0.0f)
        x += twoPi;
    const float pos = (x / twoPi) * static_cast<float> (tableSize - 1);
    return interp (sinTable, pos);
}

float LookupTables::fastCos (float x) noexcept
{
    if (tableSize == 0)
        return std::cos (x);

    const float twoPi = juce::MathConstants<float>::twoPi;
    x = std::fmod (x, twoPi);
    if (x < 0.0f)
        x += twoPi;
    const float pos = (x / twoPi) * static_cast<float> (tableSize - 1);
    return interp (cosTable, pos);
}

float LookupTables::fastTanh (float x) noexcept
{
    if (tableSize == 0)
        return std::tanh (x);

    const float range = 4.0f;
    x = juce::jlimit (-range, range, x);
    const float pos = ((x + range) / (2.0f * range)) * static_cast<float> (tableSize - 1);
    return interp (tanhTable, pos);
}

float LookupTables::fastExp (float x) noexcept
{
    if (tableSize == 0)
        return std::exp (x);

    const float range = 6.0f;
    if (expTable.empty())
    {
        expTable.resize (tableSize);
        for (int i = 0; i < tableSize; ++i)
        {
            const float val = juce::jmap (static_cast<float> (i), 0.f, static_cast<float> (tableSize - 1), -range, range);
            expTable[i] = std::exp (val);
        }
    }

    x = juce::jlimit (-range, range, x);
    const float pos = juce::jmap (x, -range, range, 0.f, static_cast<float> (tableSize - 1));
    return interp (expTable, pos);
}

float LookupTables::fastLog (float x) noexcept
{
    if (tableSize == 0)
        return std::log (x);

    const float minV = 0.0001f;
    const float maxV = 16.0f;
    if (logTable.empty())
    {
        logTable.resize (tableSize);
        for (int i = 0; i < tableSize; ++i)
        {
            const float val = juce::jmap (static_cast<float> (i), 0.f, static_cast<float> (tableSize - 1), minV, maxV);
            logTable[i] = std::log (val);
        }
    }

    x = juce::jlimit (minV, maxV, x);
    const float pos = juce::jmap (x, minV, maxV, 0.f, static_cast<float> (tableSize - 1));
    return interp (logTable, pos);
}

float LookupTables::fastPow (float x, float exponent) noexcept
{
    if (tableSize == 0)
        return std::pow (x, exponent);

    const float range = 4.0f;
    int key = static_cast<int> (std::round (exponent * 1000.0f));
    auto& table = powTables[key];
    if (table.empty())
    {
        table.resize (tableSize);
        for (int i = 0; i < tableSize; ++i)
        {
            const float val = juce::jmap (static_cast<float> (i), 0.f, static_cast<float> (tableSize - 1), -range, range);
            table[i] = std::pow (val, exponent);
        }
    }

    x = juce::jlimit (-range, range, x);
    const float pos = juce::jmap (x, -range, range, 0.f, static_cast<float> (tableSize - 1));
    return interp (table, pos);
}

void LookupTables::prepareFromScript (const juce::String& script)
{
    if (script.containsIgnoreCase ("exp("))
        (void) fastExp (0.f);
    if (script.containsIgnoreCase ("log("))
        (void) fastLog (1.f);

    juce::StringArray tokens;
    tokens.addTokens (script, " ,()", "");
    for (int i = 0; i + 2 < tokens.size(); ++i)
    {
        if (tokens[i].equalsIgnoreCase ("pow"))
        {
            float expo = tokens[i + 2].getFloatValue();
            if (std::abs (expo - std::round (expo)) > 0.001f)
                (void) fastPow (0.f, expo);
        }
    }
}

