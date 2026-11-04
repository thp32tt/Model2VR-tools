#pragma once
// The first successfully hooked real D3D9 device owns the Daytona frame
// epoch. COM vtable detours may receive calls from other D3D9 devices:
// those calls must execute once on the original device but never contribute
// eye replay, CPU-world Present commits, or frame/health counters.
#include <atomic>
#include <cstdint>
namespace m2vr::drawprobe {
class OriginalD3D9DeviceOwner {
public:
    bool BindOnce(const void* device) noexcept {
        if(!device)return false;
        std::uintptr_t empty=0u;
        return primary_.compare_exchange_strong(
            empty,reinterpret_cast<std::uintptr_t>(device),
            std::memory_order_release,std::memory_order_relaxed);
    }
    bool Owns(const void* device) const noexcept {
        return device && reinterpret_cast<std::uintptr_t>(device)==
            primary_.load(std::memory_order_acquire);
    }
    bool Bound()const noexcept {
        return primary_.load(std::memory_order_acquire)!=0u;
    }
private:
    std::atomic<std::uintptr_t> primary_{0u};
};
} // namespace m2vr::drawprobe
