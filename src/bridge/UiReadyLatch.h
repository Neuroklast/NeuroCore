#pragma once

#include <atomic>

namespace bridge
{

/** Host→UI traffic is illegal until the WebView sends UI_READY. */
class UiReadyLatch
{
public:
    void markReady() noexcept { ready.store (true, std::memory_order_release); }
    bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
    bool allowOutbound() const noexcept { return isReady(); }

private:
    std::atomic<bool> ready { false };
};

} // namespace bridge
