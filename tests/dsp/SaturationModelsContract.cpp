#include "../../src/dsp/SaturationModels.h"
#include <array>
#include <iostream>
#include <limits>
#include <vector>

static std::vector<float> render(double rate, int mode, float tone, float dynamics,
                                 float character, bool autoGain = false)
{
    neurokore::saturation::Processor dsp;
    dsp.prepare(rate);
    std::vector<float> result(8192);
    for (size_t i = 0; i < result.size(); ++i)
    {
        float frame[2] { float(0.7 * std::sin(i * 6.283185307179586 * 3100.0 / rate)), 0.f };
        dsp.process(frame, 2, mode, 4.f, 0.f, dynamics, tone, character, 1.f, 1.f, autoGain);
        result[i] = frame[0];
        if (frame[1] != 0.f) throw "stereo leakage";
    }
    return result;
}

static double distance(const std::vector<float>& a, const std::vector<float>& b)
{
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i] - b[i]);
    return sum / a.size();
}

int main()
{
    using namespace neurokore::saturation;
    const std::array<float (*)(float), 5> models {
        tapeCurve, tubeCurve, transistorCurve, transformerCurve, diodeCurve };
    int failures = 0;
    for (size_t mode = 0; mode < models.size(); ++mode)
    {
        auto curve = models[mode];
        float largestJump = 0.f;
        bool finite = true;
        for (int i = -20000; i < 20000; ++i)
        {
            const float x = i * 0.0001f;
            largestJump = std::max(largestJump, std::abs(curve(x + 0.0001f) - curve(x)));
            finite &= std::isfinite(curve(x));
        }
        finite &= std::isfinite(curve(std::numeric_limits<float>::max()));
        finite &= std::isfinite(curve(-std::numeric_limits<float>::max()));
        if (!finite || largestJump > 0.0003f || curve(0.f) != 0.f)
        {
            std::cerr << "model " << mode << ": finite=" << finite
                      << " max jump=" << largestJump << '\n';
            ++failures;
        }
    }
    for (double rate : { 44100., 48000., 96000., 192000. })
    {
        const auto reference = render(rate, 0, 0.5f, 0.5f, 0.5f);
        for (const auto& changed : {
            render(rate, 0, 1.f, 0.5f, 0.5f),
            render(rate, 0, 0.5f, 0.f, 0.5f),
            render(rate, 0, 0.5f, 0.5f, 1.f),
            render(rate, 0, 0.5f, 0.5f, 0.5f, true) })
            if (distance(reference, changed) < 0.001) ++failures;
        for (int mode = 0; mode < 5; ++mode)
        {
            for (int other = mode + 1; other < 5; ++other)
                if (distance(render(rate, mode, 0.5f, 0.5f, 0.5f),
                             render(rate, other, 0.5f, 0.5f, 0.5f)) < 0.001) ++failures;
            Processor dsp;
            dsp.prepare(rate);
            float residual = 0.f;
            for (int i = 0; i < int(rate * 2); ++i)
            {
                float frame[2] { 0.f, 0.f };
                dsp.process(frame, 2, mode, 64.f, 0.9f, 1.f, 1.f, 1.f, 4.f, 1.f, false);
                residual = std::max(std::abs(frame[0]), std::abs(frame[1]));
            }
            if (residual > 1e-5f) ++failures;
            dsp.reset();
            for (float input : { 0.f, 1e-30f, 32.f, std::numeric_limits<float>::max() })
            {
                float frame[2] { input, -input };
                dsp.process(frame, 2, mode, 64.f, 1.f, 0.f, 1.f, 1.f, 4.f, 0.5f, false);
                if (!std::isfinite(frame[0]) || !std::isfinite(frame[1])) ++failures;
            }
            dsp.prepare(rate);
            float dry[2] { 4.f, -8.f };
            dsp.process(dry, 2, mode, 64.f, 1.f, 1.f, 0.f, 1.f, 4.f, 0.f, true);
            if (dry[0] != 4.f || dry[1] != -8.f) ++failures;
        }
    }
    std::cout << "Saturation curve contract failures: " << failures << '\n';
    return failures ? 1 : 0;
}
