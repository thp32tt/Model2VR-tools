#pragma once
// New Win64 OpenXR host process QPC generation, recorded before it can
// publish tracked eyes to an original Win32 D3D9 producer.
// Old original-game pixels can survive for 125 ms, so numerical 32-bit
// pose tag reuse after host restart must not relabel that old image pair.
// No Windows dependencies or on-wire ABI changes.
#include <cstdint>
namespace m2vr::xrepoch {
inline constexpr bool WithinCurrentHost(std::uint64_t source_qpc,
                                         std::uint64_t host_start_qpc) noexcept {
    return host_start_qpc!=0u && source_qpc!=0u &&
           source_qpc>=host_start_qpc;
}
} // namespace m2vr::xrepoch
