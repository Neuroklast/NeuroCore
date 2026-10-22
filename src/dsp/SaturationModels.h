#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace neurokore::saturation
{
// Descriptive saturation models, not circuit or hardware emulations.
inline float tapeCurve(float x) noexcept { return x / std::hypot(1.f, x); }
inline float tubeCurve(float x) noexcept
{
    const float headroom = x >= 0.f ? 1.2f : 0.8f;
    return std::copysign(-headroom * std::expm1(-std::abs(x) / headroom), x);
}
inline float transistorCurve(float x) noexcept
{
    const double a = std::abs(static_cast<double>(x));
    return static_cast<float>(x / std::cbrt(1.0 + a * a * a));
}
inline float transformerCurve(float x) noexcept
{
    constexpr double halfPi = 1.5707963267948966;
    return static_cast<float>(std::atan(halfPi * x) / halfPi);
}
inline float diodeCurve(float x) noexcept
{
    const float a = std::abs(x);
    constexpr float knee = 0.45f;
    if (a <= knee) return x;
    const float headroom = x >= 0.f ? 0.25f : 0.4f;
    return std::copysign(knee - headroom * std::expm1(-(a - knee) / headroom), x);
}
inline float transfer(int mode, float x) noexcept
{
    switch (mode)
    {
        case 0: return tapeCurve(x);
        case 2: return transistorCurve(x);
        case 3: return transformerCurve(x);
        case 4: return diodeCurve(x);
        default: return tubeCurve(x);
    }
}

// Fixed storage, stereo-linked envelope, sample-rate-dependent filters.
// Parameters are smoothed by the owning block. No allocations or expression evaluation here.
class Processor
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
        attack = -std::expm1(-1.0 / (0.005 * sampleRate));
        release = -std::expm1(-1.0 / (0.100 * sampleRate));
        dcR = std::exp(-6.283185307179586 * 5.0 / sampleRate);
        reset();
    }
    void reset() noexcept
    {
        low = {}; dcIn = {}; dcOut = {}; envelope = 0.0;
        stride = 0; cachedTone = -1.f;
    }
    void process(float* frame, int channels, int mode, float drive, float bias,
                 float dynamics, float tone, float character, float output,
                 float mix, bool autoGain) noexcept
    {
        channels = std::clamp(channels, 0, 2);
        if (stride == 0 && tone != cachedTone)
        {
            cachedTone = tone;
            const double cutoff = std::min(sampleRate * 0.45, 300.0 * std::pow(60.0, tone));
            toneA = -std::expm1(-6.283185307179586 * cutoff / sampleRate);
        }
        stride = (stride + 1) % 16;
        double peak = 0.0;
        for (int c = 0; c < channels; ++c) peak = std::max(peak, std::abs(double(frame[c])));
        envelope += (peak - envelope) * (peak > envelope ? attack : release);
        const double gain = drive / (1.0 + 0.5 * dynamics * envelope);
        const double shape = 0.5 + 1.5 * character;
        const double compensation = autoGain ? 1.0 / std::max(1.f, drive) : 1.0;
        for (int c = 0; c < channels; ++c)
        {
            const double dry = frame[c];
            // Curves are bounded; saturate only the double->float conversion at the numeric limit.
            const double x = (dry * gain + bias) * shape;
            const double limit = std::numeric_limits<float>::max();
            double wet = transfer(mode, static_cast<float>(std::clamp(x, -limit, limit))) / shape;
            low[c] += toneA * (wet - low[c]);
            // Transformer has an additional low-frequency-dependent nonlinear stage.
            if (mode == 3) wet = transformerCurve(static_cast<float>(wet + character * low[c])) / (1.0 + character);
            else wet = low[c];
            const double dc = wet - dcIn[c] + dcR * dcOut[c];
            dcIn[c] = wet; dcOut[c] = dc;
            wet = dc * output * compensation;
            const double result = dry * (1.0 - mix) + wet * mix;
            frame[c] = static_cast<float>(std::clamp(result, -limit, limit));
        }
    }
private:
    double sampleRate = 44100.0, attack = 0.0, release = 0.0, dcR = 0.0;
    double envelope = 0.0, toneA = 1.0;
    std::array<double, 2> low {}, dcIn {}, dcOut {};
    int stride = 0;
    float cachedTone = -1.f;
};
}
