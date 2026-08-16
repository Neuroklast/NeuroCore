#pragma once
#include <JuceHeader.h>
#include "Config.h"
#include <atomic>
#include <cmath>

/** Audio-thread CPU watchdog. Trip → lock-free dry, then one probe after retry.
    Load is secondsUsed / host-block budget. Trips use an EMA, never a single spike.
    Never allocates. */
class CpuProtect
{
public:
    void reset() noexcept
    {
        consecutiveSoft = 0;
        consecutiveHard = 0;
        warmupRemainSec = (double) Config::kCpuWarmupSeconds;
        cooldownSec = 0.0;
        emaLoad = 0.f;
        tripped.store (false, std::memory_order_release);
        lastLoad.store (0.f, std::memory_order_relaxed);
        smoothedLoad.store (0.f, std::memory_order_relaxed);
    }

    void clear() noexcept
    {
        consecutiveSoft = 0;
        consecutiveHard = 0;
        warmupRemainSec = (double) Config::kCpuWarmupSeconds;
        cooldownSec = 0.0;
        emaLoad = 0.f;
        tripped.store (false, std::memory_order_release);
        smoothedLoad.store (0.f, std::memory_order_relaxed);
    }

    /** While tripped: count down dry time. True = run one wet probe block. */
    bool shouldProbeWet (int numSamples, double sampleRate) noexcept
    {
        if (! tripped.load (std::memory_order_acquire))
            return false;
        const double sr = (sampleRate > 1.0) ? sampleRate : Config::kDefaultSampleRate;
        const double dt = (double) juce::jmax (1, numSamples) / sr;
        cooldownSec -= dt;
        // Dry blocks use ~0 CPU — decay EMA so one cheap probe can recover.
        emaLoad += Config::kCpuEmaAlpha * (0.f - emaLoad);
        smoothedLoad.store (emaLoad, std::memory_order_relaxed);
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

        emaLoad += Config::kCpuEmaAlpha * (load - emaLoad);
        smoothedLoad.store (emaLoad, std::memory_order_relaxed);

        // Cold start after prepare/OS/IR — ignore trips for a wall-time window.
        if (warmupRemainSec > 0.0)
        {
            warmupRemainSec -= budgetSec;
            if (warmupRemainSec < 0.0)
                warmupRemainSec = 0.0;
            return;
        }

        if (tripped.load (std::memory_order_relaxed))
        {
            if (emaLoad < Config::kCpuRecoverRatio)
            {
                consecutiveSoft = 0;
                consecutiveHard = 0;
                cooldownSec = 0.0;
                tripped.store (false, std::memory_order_release);
            }
            else
            {
                cooldownSec = (double) Config::kCpuRetrySec;
            }
            return;
        }

        if (emaLoad >= Config::kCpuTripHardRatio)
        {
            if (++consecutiveHard >= Config::kCpuHardTripHits)
            {
                trip();
                return;
            }
        }
        else
        {
            consecutiveHard = 0;
        }

        if (emaLoad >= Config::kCpuTripRatio)
        {
            if (++consecutiveSoft >= Config::kCpuTripHits)
                trip();
        }
        else
        {
            consecutiveSoft = 0;
        }
    }

    bool isTripped() const noexcept
    {
        return tripped.load (std::memory_order_acquire);
    }

    /** Instantaneous secondsUsed / budget from the last observe(). */
    float getLastLoad() const noexcept
    {
        return lastLoad.load (std::memory_order_relaxed);
    }

    /** EMA of load — use for UI; ignores single-block spikes. */
    float getSmoothedLoad() const noexcept
    {
        return smoothedLoad.load (std::memory_order_relaxed);
    }

private:
    void trip() noexcept
    {
        consecutiveSoft = 0;
        consecutiveHard = 0;
        cooldownSec = (double) Config::kCpuRetrySec;
        tripped.store (true, std::memory_order_release);
    }

    int consecutiveSoft { 0 };
    int consecutiveHard { 0 };
    double warmupRemainSec { (double) Config::kCpuWarmupSeconds };
    double cooldownSec { 0.0 };
    float emaLoad { 0.f };
    std::atomic<bool>  tripped      { false };
    std::atomic<float> lastLoad     { 0.f };
    std::atomic<float> smoothedLoad { 0.f };
};
