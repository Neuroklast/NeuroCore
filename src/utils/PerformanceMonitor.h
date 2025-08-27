#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <chrono>
#include <atomic>
#include <vector>

/**
    @class PerformanceMonitor
    @brief Performance monitoring utility for audio processing bottleneck detection.
    
    Tracks processing times, CPU usage, and memory allocations to identify
    performance issues and ensure stable real-time operation.
*/
class PerformanceMonitor
{
public:
    static PerformanceMonitor& getInstance();
    
    /**
        @class ScopedTimer
        @brief RAII timer for measuring operation duration.
    */
    class ScopedTimer
    {
    public:
        ScopedTimer(const char* operationName);
        ~ScopedTimer();
        
    private:
        const char* name;
        std::chrono::high_resolution_clock::time_point startTime;
    };
    
    /**
        Record a processing time measurement
        @param operationName Name of the operation being measured
        @param durationMs Duration in milliseconds
    */
    void recordTime(const char* operationName, double durationMs);
    
    /**
        Record CPU usage measurement
        @param cpuPercent CPU usage as percentage (0-100)
    */
    void recordCpuUsage(double cpuPercent);
    
    /**
        Record memory allocation
        @param bytes Number of bytes allocated
        @param operationName Optional operation name
    */
    void recordAllocation(size_t bytes, const char* operationName = nullptr);
    
    /**
        Get average processing time for an operation
        @param operationName Name of the operation
        @return Average time in milliseconds, or -1 if not found
    */
    double getAverageTime(const char* operationName) const;
    
    /**
        Get maximum processing time for an operation
        @param operationName Name of the operation
        @return Maximum time in milliseconds, or -1 if not found
    */
    double getMaxTime(const char* operationName) const;
    
    /**
        Get current CPU usage
        @return CPU usage as percentage
    */
    double getCurrentCpuUsage() const;
    
    /**
        Get total memory allocated
        @return Total bytes allocated
    */
    size_t getTotalMemoryAllocated() const;
    
    /**
        Check if system is under heavy load
        @return true if performance is degraded
    */
    bool isUnderHeavyLoad() const;
    
    /**
        Get performance report as formatted string
        @return Detailed performance report
    */
    juce::String getPerformanceReport() const;
    
    /**
        Reset all statistics
    */
    void reset();
    
    /**
        Enable/disable performance monitoring
        @param enabled Whether to enable monitoring
    */
    void setEnabled(bool enabled) { monitoringEnabled.store(enabled); }
    
    /**
        @return true if monitoring is enabled
    */
    bool isEnabled() const { return monitoringEnabled.load(); }

private:
    struct TimingStat
    {
        std::string name;
        std::atomic<double> total{0.0};
        std::atomic<double> max{0.0};
        std::atomic<int> count{0};
        
        double getAverage() const 
        { 
            int c = count.load();
            return c > 0 ? total.load() / c : 0.0; 
        }
    };
    
    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;
    
    std::atomic<bool> monitoringEnabled{true};
    std::atomic<double> currentCpuUsage{0.0};
    std::atomic<size_t> totalMemoryAllocated{0};
    
    std::vector<TimingStat> timingStats;
    juce::CriticalSection statsMutex;
    
    TimingStat* findOrCreateStat(const char* name);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceMonitor)
};

// Convenience macro for timing operations
#define NEUROCORE_PROFILE(operationName) \
    PerformanceMonitor::ScopedTimer timer(operationName)

// Convenience macro for conditional profiling
#define NEUROCORE_PROFILE_IF(condition, operationName) \
    std::unique_ptr<PerformanceMonitor::ScopedTimer> timer; \
    if (condition) timer = std::make_unique<PerformanceMonitor::ScopedTimer>(operationName)