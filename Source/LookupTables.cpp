#include "LookupTables.h"

int LookupTables::tableSize = 0;
std::vector<float> LookupTables::sinTable;
std::vector<float> LookupTables::cosTable;
std::vector<float> LookupTables::tanhTable;

void LookupTables::initialise (int size)
{
    size = juce::jlimit (256, 4098, size);
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

