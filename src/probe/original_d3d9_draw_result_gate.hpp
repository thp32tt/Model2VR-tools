#pragma once
// Physical original x86 D3D9 Draw HRESULT integrity for authentic CPU-world
// eye frames. This does not classify Daytona gameplay/world/HUD ownership.
#include <cstdint>

namespace m2vr::vrbridge {
inline constexpr bool IsFailedOriginalD3D9Draw(
    std::uint32_t family,std::uint32_t result_bits) noexcept {
    // Original COM slots: 81 DrawPrimitive, 82 DrawIndexedPrimitive,
    // 83 DrawPrimitiveUP, 84 DrawIndexedPrimitiveUP. HRESULT is 32-bit.
    return family>=1u && family<=4u &&
           (result_bits & 0x80000000u)!=0u;
}
inline constexpr std::uint32_t FailedOriginalD3D9SourceKind(
    std::uint32_t family) noexcept {
    // Kinds 1..16 represent explicitly authenticated source adapters.
    // Physical failing original COM methods use distinct tags 17..20.
    return family>=1u && family<=4u ? 16u+family : 0u;
}
} // namespace m2vr::vrbridge
