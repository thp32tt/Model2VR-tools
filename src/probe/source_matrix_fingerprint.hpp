#pragma once
// Non-secret, content-addressed witness for an original CPU projection matrix.
// A pointer remaining equal does NOT mean its 16 floating values are stable.
// Used only for diagnostic correlation; a hash does not establish world proof.
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace m2vr::drawprobe {
inline std::uint32_t SourceMatrixFingerprint(
    const std::array<float,16>& matrix) noexcept {
    std::uint32_t hash=2166136261u; // FNV-1a over canonical float bit patterns
    for(float value:matrix){
        if(!std::isfinite(value) || std::abs(value)>1.e8f)return 0u;
        // +0 and -0 denote identical geometry, without mutating game memory.
        const std::uint32_t bits=value==0.f?0u:std::bit_cast<std::uint32_t>(value);
        for(unsigned shift=0u;shift<32u;shift+=8u){
            hash ^= (bits>>shift)&0xffu;
            hash *= 16777619u;
        }
    }
    return hash ? hash : 1u;
}
} // namespace m2vr::drawprobe
