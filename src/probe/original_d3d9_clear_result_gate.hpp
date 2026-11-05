#pragma once
// Physical original D3D9 Clear HRESULT safety. Not a world-pass classifier.
#include <cstdint>
namespace m2vr::vrbridge {
inline constexpr bool IsFailedOriginalD3D9Clear(
    std::uint32_t result_bits) noexcept {
    // Reject any failed original Clear, including invalid D3D9 clear flags.
    return (result_bits & 0x80000000u)!=0u;
}
inline constexpr std::uint32_t FailedOriginalD3D9ClearSourceKind()noexcept {
    // Draw 17..20, Clear 21: source failure, not world proof.
    return 21u;
}
} // namespace m2vr::vrbridge
