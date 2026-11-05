#pragma once
// Exact bounded identity of every *distinct* original executable Draw caller.
// Complements the older compact bloom which can saturate after many callsites.
// No original pointers, ROM/game pixels, vertices, camera data or shaders here.
// This is a NEGATIVE-control census, never independent World3D authorization.
#include <array>
#include <atomic>
#include <cstdint>
#include "original_d3d9_caller_bloom.hpp"

namespace m2vr::drawprobe {
enum class OriginalCallerInsert : std::uint8_t {
    AlreadyObserved=0u, NewCaller=1u, Incomplete=2u
};
class OriginalDrawCallerInventory final {
public:
    static constexpr std::uint32_t kSlots=4096u;
    OriginalCallerInsert Observe(std::uint32_t rva) noexcept {
        if(!rva || overflow_.load(std::memory_order_acquire)){
            overflow_.store(true,std::memory_order_release);
            return OriginalCallerInsert::Incomplete;
        }
        auto offset=MixOriginalDrawCaller(rva)&(kSlots-1u);
        for(std::uint32_t tries=0;tries<kSlots;++tries) {
            auto& cell=callers_[(offset+tries)&(kSlots-1u)];
            auto previous=cell.load(std::memory_order_acquire);
            if(previous==rva)return OriginalCallerInsert::AlreadyObserved;
            if(previous==0u && cell.compare_exchange_strong(
                previous,rva,std::memory_order_acq_rel,
                std::memory_order_acquire)){
                total_.fetch_add(1u,std::memory_order_acq_rel);
                return OriginalCallerInsert::NewCaller;
            }
            if(previous==rva)return OriginalCallerInsert::AlreadyObserved;
        }
        overflow_.store(true,std::memory_order_release);
        return OriginalCallerInsert::Incomplete;
    }
    std::uint32_t Count() const noexcept {
        return total_.load(std::memory_order_acquire);
    }
    bool Incomplete() const noexcept {
        return overflow_.load(std::memory_order_acquire);
    }
private:
    std::array<std::atomic<std::uint32_t>,kSlots> callers_{};
    std::atomic<std::uint32_t> total_{0u};
    std::atomic<bool> overflow_{false};
};
} // namespace m2vr::drawprobe
