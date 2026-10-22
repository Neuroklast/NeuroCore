#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace nk
{
/** Immutable bounded curve plus allocation-free normalized-phase playback. */
class MsegDSP
{
public:
    struct Point { double time; float value, curve; };
    static constexpr int maxPoints = 64;

    bool configure (const Point* input, int size) noexcept
    {
        if (input == nullptr || size < 2 || size > maxPoints) return false;
        for (int i = 0; i < size; ++i)
        {
            const auto& p = input[i];
            if (!std::isfinite(p.time) || !std::isfinite(p.value) || !std::isfinite(p.curve)
                || p.time < 0. || p.time > 1. || p.value < -1.f || p.value > 1.f
                || p.curve < -8.f || p.curve > 8.f || (i > 0 && p.time <= input[i-1].time)) return false;
        }
        if (input[0].time != 0. || input[size-1].time != 1.) return false;
        std::copy_n (input, size, points.begin());
        // Cache invariant segment math at configuration, never on audio.
        for (int i = 0; i < size - 1; ++i)
        {
            invSpan[(size_t) i] = 1. / (points[(size_t) (i + 1)].time - points[(size_t) i].time);
            curveExponent[(size_t) i] = std::exp2 ((double) points[(size_t) i].curve);
        }
        count = size;
        return true;
    }

    float at (double position) const noexcept
    {
        if (!std::isfinite(position) || position <= 0.) return points[0].value;
        if (position >= 1.) return points[(size_t)count-1].value;
        int lo = 0, hi = count-1;
        while (hi-lo > 1)
        {
            const int mid = (lo+hi)/2;
            if (points[(size_t)mid].time <= position) lo=mid; else hi=mid;
        }
        const auto& a=points[(size_t)lo]; const auto& b=points[(size_t)hi];
        const double t=(position-a.time)*invSpan[(size_t)lo];
        const double exponent=curveExponent[(size_t)lo];
        // Curve 0 means linear; avoid pow() entirely for that common case.
        const double shaped=exponent==1. ? t : std::pow(t,exponent);
        return a.value + (b.value-a.value)*(float)shaped;
    }

    void reset (bool start) noexcept { position=0.; active=start; started=start; previousGate=false; }
    /** Advance once/sample. Free mode ignores gate; sustain index -1 disables hold. */
    void tick (double increment, bool gate, bool free, bool loop, int sustain,
               int loopStart, int loopEnd, bool retrigger) noexcept
    {
        if (!free && gate && !previousGate && (retrigger || !active)) { position=0.; active=true; started=true; }
        previousGate=gate;
        if (!active || !std::isfinite(increment) || increment <= 0.) return;
        const double next=position+increment;
        if (!free && gate && sustain >= 0 && sustain < count && next >= points[(size_t)sustain].time
            && position <= points[(size_t)sustain].time)
        {
            position=points[(size_t)sustain].time;
            return;
        }
        const bool bounds=loopStart>=0 && loopEnd>loopStart && loopEnd<count;
        if (loop && (free || gate) && bounds && next >= points[(size_t)loopEnd].time)
        {
            const double start=points[(size_t)loopStart].time;
            const double span=points[(size_t)loopEnd].time-start;
            position=start+std::fmod(next-start,span);
        }
        else { position=std::min(1.,next); if(position>=1.) active=false; }
    }
    float value() const noexcept { return started ? at(position) : 0.f; }
    double phase() const noexcept { return position; }
    bool running() const noexcept { return active; }
    int size() const noexcept { return count; }
private:
    std::array<Point,maxPoints> points {{{0.,0.f,0.f},{1.,1.f,0.f}}};
    std::array<double,maxPoints> invSpan {1.};
    std::array<double,maxPoints> curveExponent {1.};
    int count {2};
    double position {0.};
    bool active {false}, started {false}, previousGate {false};
};
}
