#pragma once
// Original primary Device9 post-COM physical Present negative-control trace.
// Never world/HUD proof. Only four bounded Draw-family COUNTS are exported:
// no vertex/image bytes, raw resource addresses, shaders or camera matrices.
#include <cstdint>

namespace m2vr::drawprobe {
inline constexpr std::uint32_t kMaxOriginalControlDraws=10000000u;
constexpr bool OriginalControlPresentSample(
    std::uint64_t frame,std::uint32_t dp,std::uint32_t dip,
    std::uint32_t dpup,std::uint32_t dipup) noexcept {
    return frame!=0u && (frame==1u || frame%8u==0u) &&
           dp<=kMaxOriginalControlDraws &&
           dip<=kMaxOriginalControlDraws &&
           dpup<=kMaxOriginalControlDraws &&
           dipup<=kMaxOriginalControlDraws;
}
constexpr std::uint64_t OriginalControlPresentPair(
    std::uint32_t higher,std::uint32_t lower) noexcept {
    return (std::uint64_t{higher}<<32u)|lower;
}
} // namespace m2vr::drawprobe
