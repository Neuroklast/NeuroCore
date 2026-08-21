#include <JuceHeader.h>
#include "LookupTables.h"
#include "DSPUtils.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>

int LookupTables::tableSize = 0;
std::vector<float> LookupTables::sinStore;
std::vector<float> LookupTables::cosStore;
std::vector<float> LookupTables::tanhStore;
std::vector<float> LookupTables::expStore;
std::vector<float> LookupTables::logStore;
float* LookupTables::sinTable = nullptr;
float* LookupTables::cosTable = nullptr;
float* LookupTables::tanhTable = nullptr;
float* LookupTables::expTable = nullptr;
float* LookupTables::logTable = nullptr;
std::unordered_map<int, std::pair<std::vector<float>, float*>> LookupTables::powTables;
LookupTables::SmoothingOptions LookupTables::smoothing{};

const float* LookupTables::sinData() noexcept { return sinTable; }
const float* LookupTables::tanhData() noexcept { return tanhTable; }
int LookupTables::size() noexcept { return tableSize; }

void LookupTables::setSmoothingOptions (const SmoothingOptions& opts)
{
    smoothing = opts;
}

void LookupTables::initialise (int size)
{
    size = juce::jlimit (256, 4096, size);
    if (tableSize == size)
        return;

    tableSize = size;
    sinTable  = DSPUtils::alignedTable (sinStore, size);
    cosTable  = DSPUtils::alignedTable (cosStore, size);
    tanhTable = DSPUtils::alignedTable (tanhStore, size);
    expTable  = DSPUtils::alignedTable (expStore, size);
    logTable  = DSPUtils::alignedTable (logStore, size);

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

    const float expRange = 6.0f;
    for (int i = 0; i < size; ++i)
    {
        const float val = juce::jmap (static_cast<float> (i), 0.f, static_cast<float> (size - 1), -expRange, expRange);
        expTable[i] = std::exp (val);
    }

    const float logMin = 0.0001f;
    const float logMax = 16.0f;
    for (int i = 0; i < size; ++i)
    {
        const float val = juce::jmap (static_cast<float> (i), 0.f, static_cast<float> (size - 1), logMin, logMax);
        logTable[i] = std::log (val);
    }

    powTables.clear();

    std::vector<float> sinT (sinTable, sinTable + size);
    std::vector<float> cosT (cosTable, cosTable + size);
    std::vector<float> tanhT (tanhTable, tanhTable + size);
    LookupTableSmoother::smooth (sinT, smoothing);
    LookupTableSmoother::smooth (cosT, smoothing);
    LookupTableSmoother::smooth (tanhT, smoothing);
    std::copy (sinT.begin(), sinT.end(), sinTable);
    std::copy (cosT.begin(), cosT.end(), cosTable);
    std::copy (tanhT.begin(), tanhT.end(), tanhTable);
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
    return DSPUtils::lutInterp (sinTable, tableSize, pos);
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
    return DSPUtils::lutInterp (cosTable, tableSize, pos);
}

float LookupTables::fastTanh (float x) noexcept
{
    if (tableSize == 0)
        return std::tanh (x);

    const float range = 4.0f;
    x = juce::jlimit (-range, range, x);
    const float pos = ((x + range) / (2.0f * range)) * static_cast<float> (tableSize - 1);
    return DSPUtils::lutInterp (tanhTable, tableSize, pos);
}

float LookupTables::fastExp (float x) noexcept
{
    if (tableSize == 0)
        return std::exp (x);

    const float range = 6.0f;
    if (expTable == nullptr)
        initialise();

    x = juce::jlimit (-range, range, x);
    const float pos = juce::jmap (x, -range, range, 0.f, static_cast<float> (tableSize - 1));
    return DSPUtils::lutInterp (expTable, tableSize, pos);
}

float LookupTables::fastLog (float x) noexcept
{
    if (tableSize == 0)
        return std::log (x);

    const float minV = 0.0001f;
    const float maxV = 16.0f;
    if (logTable == nullptr)
        initialise();

    x = juce::jlimit (minV, maxV, x);
    const float pos = juce::jmap (x, minV, maxV, 0.f, static_cast<float> (tableSize - 1));
    return DSPUtils::lutInterp (logTable, tableSize, pos);
}

float LookupTables::fastPow (float x, float exponent) noexcept
{
    if (tableSize == 0)
        return std::pow (x, exponent);

    const float range = 4.0f;
    int key = static_cast<int> (std::round (exponent * 1000.0f));
    auto& slot = powTables[key];
    if (slot.second == nullptr)
    {
        slot.second = DSPUtils::alignedTable (slot.first, tableSize);
        for (int i = 0; i < tableSize; ++i)
        {
            const float val = juce::jmap (static_cast<float> (i), 0.f, static_cast<float> (tableSize - 1), -range, range);
            slot.second[i] = std::pow (val, exponent);
        }
    }

    x = juce::jlimit (-range, range, x);
    const float pos = juce::jmap (x, -range, range, 0.f, static_cast<float> (tableSize - 1));
    return DSPUtils::lutInterp (slot.second, tableSize, pos);
}

namespace
{
using V = juce::dsp::SIMDRegister<float>;

V lutMapAligned (const float* table, int n, const V& x, float lo, float hi) noexcept
{
    constexpr size_t w = V::SIMDNumElements;
    alignas (16) float v[w];
    x.copyToRawArray (v);
    const float span = hi - lo;
    const float last = (float) juce::jmax (1, n - 1);
    for (size_t i = 0; i < w; ++i)
    {
        const float t = juce::jlimit (lo, hi, v[i]);
        const float pos = span > 0.f ? ((t - lo) / span) * last : 0.f;
        v[i] = DSPUtils::lutInterp (table, n, pos);
    }
    return V::fromRawArray (v);
}

V lutMapWrapTwoPi (const float* table, int n, const V& x) noexcept
{
    constexpr size_t w = V::SIMDNumElements;
    alignas (16) float v[w];
    x.copyToRawArray (v);
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float last = (float) juce::jmax (1, n - 1);
    for (size_t i = 0; i < w; ++i)
    {
        float t = std::fmod (v[i], twoPi);
        if (t < 0.f)
            t += twoPi;
        v[i] = DSPUtils::lutInterp (table, n, (t / twoPi) * last);
    }
    return V::fromRawArray (v);
}
} // namespace

juce::dsp::SIMDRegister<float> LookupTables::fastSinSimd(const juce::dsp::SIMDRegister<float>& x) noexcept
{
    if (tableSize == 0 || sinTable == nullptr)
        initialise();
    return lutMapWrapTwoPi (sinTable, tableSize, x);
}

juce::dsp::SIMDRegister<float> LookupTables::fastCosSimd(const juce::dsp::SIMDRegister<float>& x) noexcept
{
    if (tableSize == 0 || cosTable == nullptr)
        initialise();
    return lutMapWrapTwoPi (cosTable, tableSize, x);
}

juce::dsp::SIMDRegister<float> LookupTables::fastTanhSimd(const juce::dsp::SIMDRegister<float>& x) noexcept
{
    if (tableSize == 0 || tanhTable == nullptr)
        initialise();
    return lutMapAligned (tanhTable, tableSize, x, -4.f, 4.f);
}

juce::dsp::SIMDRegister<float> LookupTables::fastExpSimd(const juce::dsp::SIMDRegister<float>& x) noexcept
{
    if (tableSize == 0 || expTable == nullptr)
        initialise();
    return lutMapAligned (expTable, tableSize, x, -6.f, 6.f);
}

juce::dsp::SIMDRegister<float> LookupTables::fastLogSimd(const juce::dsp::SIMDRegister<float>& x) noexcept
{
    if (tableSize == 0 || logTable == nullptr)
        initialise();
    return lutMapAligned (logTable, tableSize, x, 0.0001f, 16.f);
}

juce::dsp::SIMDRegister<float> LookupTables::fastPowSimd(const juce::dsp::SIMDRegister<float>& x, float exponent) noexcept
{
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float vals[width];
    x.copyToRawArray(vals);
    for (size_t i = 0; i < width; ++i)
        vals[i] = fastPow(vals[i], exponent);
    return juce::dsp::SIMDRegister<float>::fromRawArray(vals);
}

void LookupTables::prepareFromScript (const juce::String& script)
{
    if (tableSize == 0)
        initialise();

    (void) fastSin (0.f);
    (void) fastCos (0.f);
    (void) fastTanh (0.f);
    (void) fastExp (0.f);
    (void) fastLog (1.f);

    for (float expo : { 0.5f, 1.5f, 2.0f, 3.0f, 4.0f })
        (void) fastPow (0.f, expo);

    juce::StringArray tokens;
    tokens.addTokens (script, " ,()", "");
    for (int i = 0; i + 2 < tokens.size(); ++i)
    {
        if (tokens[i].equalsIgnoreCase ("pow"))
        {
            const float expo = tokens[i + 2].getFloatValue();
            (void) fastPow (0.f, expo);
        }
    }
}

