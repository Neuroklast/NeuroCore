#include "PerformanceMonitor.h"
#include <algorithm>

PerformanceMonitor& PerformanceMonitor::getInstance()
{
    static PerformanceMonitor instance;
    return instance;
}

PerformanceMonitor::ScopedTimer::ScopedTimer(const char* operationName)
    : name(operationName), startTime(std::chrono::high_resolution_clock::now())
{
}

PerformanceMonitor::ScopedTimer::~ScopedTimer()
{
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    double durationMs = duration.count() / 1000.0;
    
    PerformanceMonitor::getInstance().recordTime(name, durationMs);
}

void PerformanceMonitor::recordTime(const char* operationName, double durationMs)
{
    if (!monitoringEnabled.load())
        return;
        
    auto* stat = findOrCreateStat(operationName);
    if (stat)
    {
        stat->total.fetch_add(durationMs);
        stat->count.fetch_add(1);
        
        // Update max atomically
        double currentMax = stat->max.load();
        while (durationMs > currentMax && 
               !stat->max.compare_exchange_weak(currentMax, durationMs))
        {
            // Loop until we successfully update the max or find a larger value
        }
    }
}

void PerformanceMonitor::recordCpuUsage(double cpuPercent)
{
    if (monitoringEnabled.load())
    {
        currentCpuUsage.store(cpuPercent);
    }
}

void PerformanceMonitor::recordAllocation(size_t bytes, const char* operationName)
{
    if (monitoringEnabled.load())
    {
        totalMemoryAllocated.fetch_add(bytes);
        
        if (operationName)
        {
            // Record allocation as a timing stat for tracking
            recordTime((std::string(operationName) + "_alloc").c_str(), static_cast<double>(bytes));
        }
    }
}

double PerformanceMonitor::getAverageTime(const char* operationName) const
{
    juce::ScopedLock lock(statsMutex);
    
    auto it = std::find_if(timingStats.begin(), timingStats.end(),
                          [operationName](const TimingStat& stat) {
                              return stat.name == operationName;
                          });
    
    return it != timingStats.end() ? it->getAverage() : -1.0;
}

double PerformanceMonitor::getMaxTime(const char* operationName) const
{
    juce::ScopedLock lock(statsMutex);
    
    auto it = std::find_if(timingStats.begin(), timingStats.end(),
                          [operationName](const TimingStat& stat) {
                              return stat.name == operationName;
                          });
    
    return it != timingStats.end() ? it->max.load() : -1.0;
}

double PerformanceMonitor::getCurrentCpuUsage() const
{
    return currentCpuUsage.load();
}

size_t PerformanceMonitor::getTotalMemoryAllocated() const
{
    return totalMemoryAllocated.load();
}

bool PerformanceMonitor::isUnderHeavyLoad() const
{
    // Consider system under heavy load if:
    // 1. CPU usage > 80%
    // 2. Any critical operation takes > 5ms average
    // 3. Memory allocations are excessive
    
    double cpu = getCurrentCpuUsage();
    if (cpu > 80.0)
        return true;
    
    // Check critical operations
    const char* criticalOps[] = {
        "processBlock",
        "validateFormula", 
        "renderOpenGL",
        "signalChainProcess"
    };
    
    for (const char* op : criticalOps)
    {
        double avgTime = getAverageTime(op);
        if (avgTime > 5.0) // More than 5ms average is concerning
            return true;
    }
    
    // Check for excessive memory allocations (> 100MB)
    if (getTotalMemoryAllocated() > 100 * 1024 * 1024)
        return true;
    
    return false;
}

juce::String PerformanceMonitor::getPerformanceReport() const
{
    juce::ScopedLock lock(statsMutex);
    
    juce::String report;
    report += "=== NeuroCore Performance Report ===\n\n";
    
    report += "System Status:\n";
    report += "CPU Usage: " + juce::String(getCurrentCpuUsage(), 1) + "%\n";
    report += "Memory Allocated: " + juce::String(getTotalMemoryAllocated() / 1024) + " KB\n";
    report += "Under Heavy Load: " + juce::String(isUnderHeavyLoad() ? "YES" : "NO") + "\n\n";
    
    report += "Operation Timings:\n";
    report += juce::String::repeatedString("-", 60) + "\n";
    report += "Operation                    Avg (ms)    Max (ms)    Count\n";
    report += juce::String::repeatedString("-", 60) + "\n";
    
    for (const auto& stat : timingStats)
    {
        if (stat.count.load() > 0)
        {
            report += juce::String(stat.name).paddedRight(' ', 28);
            report += juce::String(stat.getAverage(), 2).paddedLeft(' ', 8);
            report += juce::String(stat.max.load(), 2).paddedLeft(' ', 12);
            report += juce::String(stat.count.load()).paddedLeft(' ', 9);
            report += "\n";
        }
    }
    
    report += juce::String::repeatedString("-", 60) + "\n\n";
    
    // Performance recommendations
    report += "Recommendations:\n";
    if (getCurrentCpuUsage() > 70.0)
        report += "• High CPU usage detected - consider reducing effect complexity\n";
    
    for (const auto& stat : timingStats)
    {
        if (stat.getAverage() > 10.0)
            report += "• Operation '" + juce::String(stat.name) + "' is slow - consider optimization\n";
    }
    
    if (getTotalMemoryAllocated() > 50 * 1024 * 1024)
        report += "• High memory usage - check for memory leaks\n";
    
    return report;
}

void PerformanceMonitor::reset()
{
    juce::ScopedLock lock(statsMutex);
    
    timingStats.clear();
    currentCpuUsage.store(0.0);
    totalMemoryAllocated.store(0);
}

PerformanceMonitor::TimingStat* PerformanceMonitor::findOrCreateStat(const char* name)
{
    juce::ScopedLock lock(statsMutex);
    
    auto it = std::find_if(timingStats.begin(), timingStats.end(),
                          [name](const TimingStat& stat) {
                              return stat.name == name;
                          });
    
    if (it != timingStats.end())
        return &(*it);
    
    // Create new stat
    TimingStat newStat;
    newStat.name = name;
    timingStats.push_back(std::move(newStat));
    
    return &timingStats.back();
}