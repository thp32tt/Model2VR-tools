#pragma once
// Once an authenticated world source Draw fails, a partially rasterized pair
// must not be republished or rebuilt for that physical original Present.
// This is frame accounting only; it never authenticates a Daytona world pass.
#include <cstdint>

namespace m2vr::vrbridge {
class OriginalSourceFrameEpoch {
public:
    constexpr bool CanBegin(std::uint64_t frame,
                            std::uint64_t completed) const noexcept {
        return frame!=0u && frame>completed && frame>rejected_;
    }
    // True only for the first rejection of this frame/epoch. Monotonic
    // original Present IDs prevent rewinding into an incomplete world.
    constexpr bool Revoke(std::uint64_t frame) noexcept {
        if(frame==0u || frame<=rejected_)return false;
        rejected_=frame;
        return true;
    }
    constexpr std::uint64_t LastRejected() const noexcept {
        return rejected_;
    }
private:
    std::uint64_t rejected_=0u;
};
} // namespace m2vr::vrbridge
