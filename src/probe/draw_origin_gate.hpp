#pragma once
// Bounded passive D3D9 caller-provenance sampling.
// For each COM Draw family, retain up to 64 previously unseen executable
// return-address buckets per process and a 1/1024 periodic observation per
// presented frame. This is *not* a gameplay/world geometry classifier.
// Atomic bookkeeping makes simultaneous draws safe without a hot-path mutex.
#include <array>
#include <atomic>
#include <cstdint>

namespace m2vr::drawprobe {
class DrawOriginGate {
public:
    static constexpr std::uint32_t kMaxNewSources=64u;
    static constexpr std::uint32_t kPeriodicMask=1023u;

    bool ShouldObserve(std::uint32_t family,
                       std::uint32_t caller_rva,
                       std::uint32_t ordinal) noexcept {
        if(family<1u || family>4u)return false;
        bool unseen=false;
        if(caller_rva){
            // 256-bit approximate set per family. A hash collision only
            // suppresses an extra first-seen observation; the periodic
            // sample remains available. No raw memory address is stored.
            std::uint32_t h=caller_rva^(caller_rva>>16);
            h*=0x7feb352du;
            h^=h>>15;
            h*=0x846ca68bu;
            h^=h>>16;
            const std::uint32_t bucket=h&255u;
            const std::uint64_t mask=std::uint64_t{1}<<(bucket&63u);
            const auto previous=
                seen_[family-1][bucket>>6].fetch_or(
                    mask,std::memory_order_relaxed);
            if(!(previous&mask)){
                const auto slot=distinct_[family-1].fetch_add(
                    1u,std::memory_order_relaxed);
                unseen=slot<kMaxNewSources;
            }
        }
        return ordinal==0u || (ordinal&kPeriodicMask)==0u || unseen;
    }
private:
    std::array<std::array<std::atomic<std::uint64_t>,4>,4> seen_{};
    std::array<std::atomic<std::uint32_t>,4> distinct_{};
};
} // namespace m2vr::drawprobe
