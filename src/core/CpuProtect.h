#pragma once
#include <JuceHeader.h>
#include "Config.h"
#include <atomic>
#include <cmath>

/** Audio-thread CPU watchdog. Trip → lock-free dry, then automatic retry.
    Never allocates. */
class CpuProtect
{
public:
    void reset() noexcept
    {
        consecutive = 0;
        warmup = Config::kCpuWarmupBlocks;
        cooldownSec = 0.0;
        tripped.store (false, std::memory_order_release);
        lastLoad.store (0.f, std::memory_order_relaxed);
    }

    void clear() noexcept
    {
        consecutive = 0;
        warmup = Config::kCpuWarmupBlocks;
        cooldownSec = 0.0;
        tripped.store (false, std::memory_order_release);
    }

    /** While tripped: count down dry time. True = run one wet probe block. */
    bool shouldProbeWet (int numSamples, double sampleRate) noexcept
    {
        if (! tripped.load (std::memory_order_acquire))
            return false;
        const double sr = (sampleRate > 1.0) ? sampleRate : Config::kDefaultSampleRate;
        const double dt = (double) juce::jmax (1, numSamples) / sr;
        cooldownSec -= dt;
        if (cooldownSec > 0.0)
            return false;
        cooldownSec = 0.0;
        return true;
    }

    /** Feed one wet block's used time vs host budget. Audio thread only. */
    void observe (double secondsUsed, double budgetSec) noexcept
    {
        if (! std::isfinite (secondsUsed) || secondsUsed < 0.0)
            secondsUsed = 0.0;
        if (! std::isfinite (budgetSec) || budgetSec < 1.0e-6)
            budgetSec = 1.0e-6;

        const float load = (float) (secondsUsed / budgetSec);
        lastLoad.store (load, std::memory_order_relaxed);

        // First blocks after prepare/clear are cold (alloc, cache) — do not trip.
        if (warmup > 0)
        {
            --warmup;
            return;
        }

        if (tripped.load (std::memory_order_relaxed))
        {
            if (load < Config::kCpuRecoverRatio)
            {
                consecutive = 0;
                cooldownSec = 0.0;
                tripped.store (false, std::memory_order_release);
            }
            else
            {
                cooldownSec = (double) Config::kCpuRetrySec;
            }
            return;
        }

        if (load >= Config::kCpuTripHardRatio)
        {
            consecutive = Config::kCpuTripHits;
            cooldownSec = (double) Config::kCpuRetrySec;
            tripped.store (true, std::memory_order_release);
            return;
        }

        if (load >= Config::kCpuTripRatio)
        {
            if (++consecutive >= Config::kCpuTripHits)
            {
                cooldownSec = (double) Config::kCpuRetrySec;
                tripped.store (true, std::memory_order_release);
            }
        }
        else
        {
            consecutive = 0;
        }
    }

    bool isTripped() const noexcept
    {
        return tripped.load (std::memory_order_acquire);
    }

    float getLastLoad() const noexcept
    {
        return lastLoad.load (std::memory_order_relaxed);
    }

private:
    int consecutive { 0 };
    int warmup { Config::kCpuWarmupBlocks };
    double cooldownSec { 0.0 };
    std::atomic<bool>  tripped  { false };
    std::atomic<float> lastLoad { 0.f };
};
