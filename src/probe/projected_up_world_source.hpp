#pragma once
// Exact real fixed-function D3D9 XYZRHW+TEX1 DrawPrimitiveUP source decoder.
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
};
inline constexpr bool ExactProjectedUPWorldFVF(
    std::uint32_t fvf,std::uint32_t stride) noexcept {
    return fvf==(D3DFVF_XYZRHW|D3DFVF_TEX1) && stride==24u;
}
inline bool DecodeProjectedUPWorldVertex(const void* raw,
    std::uint32_t fvf,std::uint32_t stride,
    ProjectedUPWorldVertex& out) noexcept {
    if(!raw || !ExactProjectedUPWorldFVF(fvf,stride))return false;
    std::array<float,6> v{};
    std::memcpy(v.data(),raw,24u); // caller already copied with x86 SEH
    for(float number:v)if(!std::isfinite(number))return false;
    if(v[3]<=0.f || v[3]>1.e6f ||
       v[2]<0.f || v[2]>1.f ||
       std::abs(v[0])>16384.f || std::abs(v[1])>16384.f ||
       std::abs(v[4])>65536.f || std::abs(v[5])>65536.f)return false;
    out.screen_xy={v[0],v[1]};
    out.uv={v[4],v[5]};
    return true;
}
} // namespace m2vr::drawprobe
