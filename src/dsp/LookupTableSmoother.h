#pragma once

#include <vector>
#include <cstddef>
#include <limits>
#include <cmath>
#include <JuceHeader.h>

namespace LookupTableSmoother
{
    enum Flags
    {
        None          = 0,
        FIR           = 1 << 0,
        Spline        = 1 << 1,
        GradientLimit = 1 << 2,
        DomainClamp   = 1 << 3
    };

    struct Options
    {
        int flags = None;
        std::size_t firWindow = 5;          // odd number preferred
        float gradientThreshold = 0.0f;     // maximum allowed step between values
        float clampMin = -std::numeric_limits<float>::infinity();
        float clampMax =  std::numeric_limits<float>::infinity();
    };

    // Apply smoothing according to options. Invalid values are replaced
    // by interpolation of neighbouring points.
    void smooth(std::vector<float>& data, const Options& options);

    // Replace NaN or Inf values by interpolation of valid neighbours.
    void replaceInvalid(std::vector<float>& data);
}

