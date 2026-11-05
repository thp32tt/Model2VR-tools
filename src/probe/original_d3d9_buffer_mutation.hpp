#pragma once
// Original x86 D3D9 resource COM Lock/Unlock source safety, not World3D proof.
#include <cstdint>
#include <array>
#include <atomic>
#include <mutex>
namespace m2vr::drawprobe {
constexpr bool IsWritableBufferLock(std::uint32_t flags) noexcept {
    return (flags & 0x10u)==0u; // D3DLOCK_READONLY=0x10
}
constexpr bool OriginalBufferFailureMethodValid(std::uint32_t id) noexcept {
    return id>=37u && id<=40u;
}
// Track outstanding original writable resource mappings without copying any
// bytes. An incomplete mapping across Draw/Present cannot prove stereo.
// Bound the set: overflow or duplicate writable locks fail closed.
class OriginalBoundWriteLocks {
public:
    bool Begin(const void* resource) noexcept {
        if(!resource)return false;
        std::lock_guard lock(mu_);
        for(auto* p:slots_)if(p==resource)return false;
        for(auto& slot:slots_)if(slot==nullptr){
            slot=resource;
            count_.fetch_add(1u,std::memory_order_release);
            return true;
        }
        return false; // too many simultaneously active game mappings
    }
    void SuccessfulUnlock(const void* resource) noexcept {
        if(!resource)return;
        std::lock_guard lock(mu_);
        for(auto& slot:slots_)if(slot==resource){
            slot=nullptr;
            count_.fetch_sub(1u,std::memory_order_release);
            return;
        }
        // READONLY and non-EXE locks may unlock without a write entry.
    }
    bool HasOutstandingWrites()const noexcept {
        return count_.load(std::memory_order_acquire)!=0u;
    }
    void AfterSuccessfulDeviceReset() noexcept {
        std::lock_guard lock(mu_);
        slots_.fill(nullptr);
        count_.store(0u,std::memory_order_release);
    }
private:
    mutable std::mutex mu_;
    std::array<const void*,64> slots_{};
    std::atomic<std::uint32_t> count_{0u};
};
} // namespace m2vr::drawprobe
