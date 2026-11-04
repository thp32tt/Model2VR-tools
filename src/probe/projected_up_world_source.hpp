#pragma once
// Exact real fixed-function D3D9 XYZRHW+TEX1 and XYZRHW+DIFFUSE+TEX1
// DrawPrimitiveUP source decoder. Packed D3DCOLOR is NOT a float.
// The source itself is already projected 2D: original thread-local CPU
// projector witnesses and a separate authenticated world pass are required
// before it can drive the calibrated 3D OpenXR raster.
#include <d3d9.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace m2vr::drawprobe {
struct ProjectedUPWorldVertex {
    std::array<float,2> screen_xy{};
    std::array<float,2> uv{};
    std::uint32_t diffuse_argb=0xffffffffu;
    bool has_diffuse=false;
};
inline constexpr bool ExactProjectedUPWorldFVF(
    std::uint32_t fvf,std::uint32_t stride) noexcept {
    return (fvf==(D3DFVF_XYZRHW|D3DFVF_TEX1) && stride==24u) ||
        (fvf==(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1) &&
         stride==28u);
}
inline bool DecodeProjectedUPWorldVertex(const void* raw,
    std::uint32_t fvf,std::uint32_t stride,
    ProjectedUPWorldVertex& out) noexcept {
    if(!raw || !ExactProjectedUPWorldFVF(fvf,stride))return false;
    const bool colored=(fvf&D3DFVF_DIFFUSE)!=0u;
    const auto* bytes=static_cast<const std::uint8_t*>(raw);
    std::array<float,4> position{};
    std::array<float,2> uv{};
    std::memcpy(position.data(),bytes,sizeof(position));
    std::memcpy(uv.data(),bytes+(colored?20u:16u),sizeof(uv));
    for(float number:position)if(!std::isfinite(number))return false;
    for(float number:uv)if(!std::isfinite(number))return false;
    if(position[3]<=0.f || position[3]>1.e6f ||
       position[2]<0.f || position[2]>1.f ||
       std::abs(position[0])>16384.f ||
       std::abs(position[1])>16384.f ||
       std::abs(uv[0])>65536.f || std::abs(uv[1])>65536.f)
        return false;
    ProjectedUPWorldVertex next{};
    next.screen_xy={position[0],position[1]};
    next.uv=uv;
    next.has_diffuse=colored;
    if(colored)std::memcpy(&next.diffuse_argb,bytes+16u,sizeof(std::uint32_t));
    out=next;
    return true;
}
} // namespace m2vr::drawprobe
