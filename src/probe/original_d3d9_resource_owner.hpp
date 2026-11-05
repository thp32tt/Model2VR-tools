#pragma once
// Genuine original D3D9 resource ownership is three-state, never inferred
// from a failed resource GetDevice. No game-private COM/pixel data.
#include <cstdint>
namespace m2vr::vrbridge {
enum class OriginalResourceOwner : std::uint8_t {
    Unknown=0u, Primary=1u, Secondary=2u
};
constexpr OriginalResourceOwner DecideOriginalResourceOwner(
    bool query_succeeded,bool returned_device,bool primary_device) noexcept {
    if(!query_succeeded || !returned_device)
        return OriginalResourceOwner::Unknown;
    return primary_device
        ?OriginalResourceOwner::Primary:OriginalResourceOwner::Secondary;
}
} // namespace m2vr::vrbridge
