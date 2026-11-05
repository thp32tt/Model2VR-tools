#pragma once
// Strict real-original surface mutation failure IDs, never world-pass proof.
#include <cstdint>
namespace m2vr::vrbridge {
// 1=UpdateSurface (COM 30), 2=StretchRect (34), 3=ColorFill (35).
inline constexpr bool IsFailedOriginalD3D9Surface(
    std::uint32_t method,std::uint32_t result_bits)noexcept {
    return method>=1u && method<=3u && (result_bits&0x80000000u)!=0u;
}
inline constexpr std::uint32_t FailedOriginalD3D9SurfaceSourceKind(
    std::uint32_t method)noexcept {
    // Separate from Draw 17..20 and Clear 21.
    return method>=1u && method<=3u?21u+method:0u;
}
} // namespace m2vr::vrbridge
