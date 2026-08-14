#pragma once

#include <JuceHeader.h>
#include <cmath>

/** Peak / RMS / correlation from a stereo snapshot (UI thread, no alloc). */
namespace ScopeAnalytics
{
    struct StereoStats
    {
        float peakL = 0.f;
        float peakR = 0.f;
        float rmsL  = 0.f;
        float rmsR  = 0.f;
        float peakDbL = -100.f;
        float peakDbR = -100.f;
        float rmsDbL  = -100.f;
        float rmsDbR  = -100.f;
        /** −1 = inverted, 0 = uncorrelated / silent, +1 = mono. */
        float correlation = 0.f;
        /** 0 = mid only, 1 = side only. */
        float width = 0.f;
    };

    inline float linearToDbClamped (float lin) noexcept
    {
        if (! std::isfinite (lin) || lin <= 1.0e-12f)
            return -100.f;
        const float db = 20.f * std::log10 (lin);
        return juce::jlimit (-100.f, 12.f, db);
    }

    inline StereoStats analyse (const float* left, const float* right, int n) noexcept
    {
        StereoStats s;
        if (n <= 0 || left == nullptr)
            return s;

        const float* rch = right != nullptr ? right : left;
        double accL = 0.0, accR = 0.0, accLR = 0.0, accM = 0.0, accS = 0.0;
        float pL = 0.f, pR = 0.f;

        for (int i = 0; i < n; ++i)
        {
            const float l = std::isfinite (left[i]) ? left[i] : 0.f;
            const float r = std::isfinite (rch[i]) ? rch[i] : 0.f;
            pL = juce::jmax (pL, std::abs (l));
            pR = juce::jmax (pR, std::abs (r));
            accL  += (double) l * (double) l;
            accR  += (double) r * (double) r;
            accLR += (double) l * (double) r;
            const double m = 0.5 * ((double) l + (double) r);
            const double sd = 0.5 * ((double) l - (double) r);
            accM += m * m;
            accS += sd * sd;
        }

        const double invN = 1.0 / (double) n;
        s.peakL = pL;
        s.peakR = pR;
        s.rmsL  = (float) std::sqrt (accL * invN);
        s.rmsR  = (float) std::sqrt (accR * invN);
        s.peakDbL = linearToDbClamped (pL);
        s.peakDbR = linearToDbClamped (pR);
        s.rmsDbL  = linearToDbClamped (s.rmsL);
        s.rmsDbR  = linearToDbClamped (s.rmsR);

        const double denom = std::sqrt (accL * accR);
        s.correlation = denom > 1.0e-20
                            ? (float) juce::jlimit (-1.0, 1.0, accLR / denom)
                            : 0.f;
        const double ms = accM + accS;
        s.width = ms > 1.0e-20 ? (float) (accS / ms) : 0.f;
        return s;
    }

    /** Goniometer: x = side, y = −mid (up is in-phase). */
    inline juce::Point<float> gonioPoint (float l, float r) noexcept
    {
        if (! std::isfinite (l)) l = 0.f;
        if (! std::isfinite (r)) r = 0.f;
        return { 0.5f * (l - r), -0.5f * (l + r) };
    }
}
