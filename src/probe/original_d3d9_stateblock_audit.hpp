#pragma once
// Bounded metadata-only physical original StateBlock Apply/observer events.
// Every StateBlock Apply independently revokes speculative binocular eyes;
// only the first mutation per physical Present needs an audit record.
#include <atomic>
#include <cstdint>

namespace m2vr::drawprobe {
class OriginalStateBlockAuditOncePerPresent final {
public:
    bool Claim(std::uint64_t physical_present) noexcept {
        if(!physical_present)return false;
        auto prior=last_reported_.load(std::memory_order_acquire);
        while(prior<physical_present){
            if(last_reported_.compare_exchange_weak(
                   prior,physical_present,std::memory_order_acq_rel,
                   std::memory_order_acquire))return true;
        }
        return false;
    }
private:
    std::atomic<std::uint64_t> last_reported_{0};
};
} // namespace m2vr::drawprobe
