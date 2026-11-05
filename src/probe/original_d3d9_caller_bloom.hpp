#pragma once
// Bounded, per-process ORIGINAL primary Device9 Draw caller fingerprint.
// All four physical Draw families contribute, including UNSAMPLED UP calls.
// Only EXE-relative return RVAs are mixed; no game code, geometry, image,
// camera, ROM, pointer, shader or FFB bytes are inspected or transmitted.
// Hash collisions intentionally cause conservative false-positive exclusions
// in private nonrace controls; this is NEVER gameplay World3D proof.
#include <cstdint>

namespace m2vr::drawprobe {
struct OriginalAllFamilyCallerBloom {
    std::uint64_t lane0=0u;
    std::uint64_t lane1=0u;
};
constexpr std::uint32_t MixOriginalDrawCaller(std::uint32_t rva) noexcept {
    rva ^= rva >> 16u;
    rva *= 0x7feb352du;
    rva ^= rva >> 15u;
    rva *= 0x846ca68bu;
    return rva ^ (rva >> 16u);
}
constexpr OriginalAllFamilyCallerBloom OriginalDrawCallerBits(
    std::uint32_t rva) noexcept {
    if(!rva)return {};
    return {
        std::uint64_t{1} << (MixOriginalDrawCaller(rva) & 63u),
        std::uint64_t{1} << (MixOriginalDrawCaller(rva ^ 0x9e3779b9u) & 63u)
    };
}
constexpr bool OriginalDrawCallerCouldBeInBloom(
    OriginalAllFamilyCallerBloom bloom,std::uint32_t rva) noexcept {
    const auto bits=OriginalDrawCallerBits(rva);
    return bits.lane0!=0u && (bloom.lane0&bits.lane0)==bits.lane0 &&
           (bloom.lane1&bits.lane1)==bits.lane1;
}
} // namespace m2vr::drawprobe
