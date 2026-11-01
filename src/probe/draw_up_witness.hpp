#pragma once
// Bounded observation of D3D9 UP XYZ/RHW vertices. No D3D9 state or game
// memory is modified; no automatic world/stereo classification is permitted.
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
namespace m2vr::drawprobe {
struct VertexWitness { float x=0, y=0, z=0, rhw=0; };
constexpr std::uint32_t MaxVertices=3u;
constexpr bool ValidStride(std::uint32_t stride) noexcept {
    return stride>=16u && stride<=256u && (stride%4u)==0u;
}
constexpr std::uint32_t PrimitiveElements(std::uint32_t type,std::uint32_t count) noexcept {
    if(!count || count>8192u)return 0;
    switch(type){
    case 1:return count; case 2:return 2u*count;
    case 3:return count+1u; case 4:return count*3u;
    case 5:case 6:return count+2u;
    default:return 0;
    }
}
inline bool DecodeXYZRHW(const std::array<std::uint32_t,4>& bits,
                         VertexWitness& out) noexcept {
    const VertexWitness v{std::bit_cast<float>(bits[0]),
                          std::bit_cast<float>(bits[1]),
                          std::bit_cast<float>(bits[2]),
                          std::bit_cast<float>(bits[3])};
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||
       !std::isfinite(v.z)||!std::isfinite(v.rhw)||v.rhw<=0)return false;
    out=v;return true;
}
inline std::uint64_t PackPair(float hi,float lo) noexcept {
    return (std::uint64_t{std::bit_cast<std::uint32_t>(hi)}<<32) |
        std::bit_cast<std::uint32_t>(lo);
}
} // namespace m2vr::drawprobe
