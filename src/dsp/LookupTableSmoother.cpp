#include "LookupTableSmoother.h"

namespace LookupTableSmoother
{
    namespace
    {
        static void movingAverage(std::vector<float>& data, std::size_t window)
        {
            if (window < 2 || data.empty())
                return;
            if ((window & 1u) == 0)
                ++window; // ensure odd window
            std::vector<float> out(data.size());
            const int half = static_cast<int>(window / 2);
            const int size = static_cast<int>(data.size());
            for (int i = 0; i < size; ++i)
            {
                float sum = 0.f;
                int count = 0;
                for (int j = -half; j <= half; ++j)
                {
                    int idx = juce::jlimit(0, size - 1, i + j);
                    sum += data[(std::size_t)idx];
                    ++count;
                }
                out[(std::size_t)i] = sum / static_cast<float>(count);
            }
            data.swap(out);
        }

        static void splineSmooth(std::vector<float>& data)
        {
            if (data.size() < 4)
                return;
            std::vector<float> out(data.size());
            const int size = static_cast<int>(data.size());
            for (int i = 0; i < size; ++i)
            {
                int i0 = juce::jlimit(0, size - 1, i - 1);
                int i1 = i;
                int i2 = juce::jlimit(0, size - 1, i + 1);
                int i3 = juce::jlimit(0, size - 1, i + 2);
                float p0 = data[(std::size_t)i0];
                float p1 = data[(std::size_t)i1];
                float p2 = data[(std::size_t)i2];
                float p3 = data[(std::size_t)i3];
                float t  = 0.5f;
                float t2 = t * t;
                float t3 = t2 * t;
                out[(std::size_t)i] = 0.5f * ((2.f * p1) + (-p0 + p2) * t +
                                             (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 +
                                             (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
            }
            data.swap(out);
        }

        static void limitGradient(std::vector<float>& data, float maxStep)
        {
            if (maxStep <= 0.f || data.empty())
                return;
            for (std::size_t i = 1; i < data.size(); ++i)
            {
                float diff = data[i] - data[i - 1];
                if (diff > maxStep)
                    data[i] = data[i - 1] + maxStep;
                else if (diff < -maxStep)
                    data[i] = data[i - 1] - maxStep;
            }
        }

        static void clamp(std::vector<float>& data, float minV, float maxV)
        {
            for (auto& v : data)
                v = juce::jlimit(minV, maxV, v);
        }
    } // namespace

    void replaceInvalid(std::vector<float>& data)
    {
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            if (! std::isfinite(data[i]))
            {
                float prev = (i > 0) ? data[i - 1] : 0.f;
                float next = (i + 1 < data.size()) ? data[i + 1] : prev;
                data[i] = 0.5f * (prev + next);
            }
        }
    }

    void smooth(std::vector<float>& data, const Options& opt)
    {
        if (opt.flags & DomainClamp)
            clamp(data, opt.clampMin, opt.clampMax);
        if (opt.flags & FIR)
            movingAverage(data, opt.firWindow);
        if (opt.flags & Spline)
            splineSmooth(data);
        if (opt.flags & GradientLimit)
            limitGradient(data, opt.gradientThreshold);
        replaceInvalid(data);
    }
} // namespace LookupTableSmoother

