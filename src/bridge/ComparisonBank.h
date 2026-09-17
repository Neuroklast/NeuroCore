#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>

namespace bridge
{
/** Processor-lifetime A/B scratch states. Message thread only; never used by processBlock. */
class ComparisonBank
{
public:
    struct Snapshot { juce::MemoryBlock state; juce::String script; };
    using Capture = std::function<Snapshot()>;
    using Restore = std::function<bool(const Snapshot&)>;
    int active() const noexcept { return selected; }
    bool has(int index) const noexcept { return index >= 0 && index < 2 && valid[(size_t) index]; }
    bool switchTo(int index, const Capture& capture, const Restore& restore)
    {
        if (index < 0 || index > 1) return false;
        if (index == selected) return true;
        auto current = capture();
        const auto target = has(index) ? slots[(size_t) index] : current;
        if (!restore(target)) return false;
        slots[(size_t) selected] = std::move(current);
        valid[(size_t) selected] = true;
        slots[(size_t) index] = target;
        valid[(size_t) index] = true;
        selected = index;
        return true;
    }
    void copyToOther(const Capture& capture)
    {
        slots[(size_t) (1 - selected)] = capture();
        valid[(size_t) (1 - selected)] = true;
    }
private:
    std::array<Snapshot, 2> slots;
    std::array<bool, 2> valid { false, false };
    int selected = 0;
};
}
