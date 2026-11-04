#pragma once
// A same-thread CPU projector/Draw join must preserve the ORIGINAL
// emulator matrix and active geometry command up to the actual GPU Draw.
// This is additional fail-closed source provenance, not world/HUD proof.
#include <cstdint>

namespace m2vr::drawprobe {
inline constexpr bool SameOriginalCpuSourceAtDraw(
    std::uint32_t expected_matrix,
    std::uint32_t expected_fingerprint,
    std::uint32_t expected_command,
    std::uint32_t actual_matrix,
    std::uint32_t actual_fingerprint,
    bool actual_command_readable,
    std::uint32_t actual_command) noexcept {
    return expected_matrix!=0u && expected_fingerprint!=0u &&
        actual_command_readable &&
        expected_matrix==actual_matrix &&
        expected_fingerprint==actual_fingerprint &&
        expected_command==actual_command;
}
} // namespace m2vr::drawprobe
