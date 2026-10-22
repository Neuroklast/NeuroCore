#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

namespace neurokore
{
// 129-tap type-III windowed Hilbert transformer. The real path and dry signal
// share its 64-sample delay. Finite transition bands near DC and Nyquist are
// intentional; this is not a pitch shifter and is not an ideal broadband Hilbert transform.
class FrequencyShifterDSP
{
public:
    static constexpr int latency = 64;
    static constexpr int length = 129;
    void prepare(double rate) noexcept
    {
        sr = rate > 0. ? rate : 44100.;
        constexpr double pi = 3.141592653589793;
        for (int n = 1; n < latency; n += 2)
        {
            const double window = .42 - .5 * std::cos(2. * pi * n / (length - 1))
                                       + .08 * std::cos(4. * pi * n / (length - 1));
            coefficients[n / 2] = 2. * window / (pi * (n - latency));
        }
        reset();
    }
    void reset() noexcept { history = {}; cursor = 0; phase = 0.; }
    void process(float* frame, int channels, double hz, float mix) noexcept
    {
        constexpr double tau = 6.283185307179586;
        const double cosine = std::cos(phase), sine = std::sin(phase);
        for (int c = 0; c < std::clamp(channels, 0, 2); ++c)
        {
            auto& ring = history[c];
            ring[cursor] = frame[c];
            // All offsets are bounded by one revolution of the 129-slot
            // ring. Replace the 64 variable modulo operations per channel
            // with two wrap-checked cursors walking in opposite directions.
            // This is bit-for-bit equivalent to the original FIR order.
            int realIndex = cursor - latency;
            if (realIndex < 0) realIndex += length;
            const double real = ring[(size_t) realIndex];
            int left = cursor - 1;
            if (left < 0) left += length;
            int right = cursor + 2;
            if (right >= length) right -= length;
            double quadrature = 0.;
            for (int k = 0; k < latency / 2; ++k)
            {
                quadrature += coefficients[(size_t) k]
                    * (ring[(size_t) left] - ring[(size_t) right]);
                left -= 2;
                if (left < 0) left += length;
                right += 2;
                if (right >= length) right -= length;
            }
            const double wet = real * cosine - quadrature * sine;
            constexpr double limit = std::numeric_limits<float>::max();
            frame[c] = static_cast<float>(std::clamp(real * (1. - mix) + wet * mix, -limit, limit));
        }
        cursor = (cursor + 1) % length;
        phase = std::remainder(phase + tau * hz / sr, tau);
    }
private:
    double sr = 44100., phase = 0.;
    int cursor = 0;
    std::array<double, 32> coefficients {};
    std::array<std::array<double, length>, 2> history {};
};
}
