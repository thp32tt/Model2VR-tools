#pragma once
// Original x86 D3D9 resource COM Lock/Unlock source safety, not World3D proof.
#include <cstdint>
namespace m2vr::drawprobe {
constexpr bool IsWritableBufferLock(std::uint32_t flags) noexcept {
    return (flags & 0x10u)==0u; // D3DLOCK_READONLY=0x10
}
constexpr bool OriginalBufferFailureMethodValid(std::uint32_t id) noexcept {
    return id>=37u && id<=40u;
}
} // namespace m2vr::drawprobe
