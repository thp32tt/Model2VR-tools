#pragma once
// Explicit screen-space triangle raster state for verified D3D9 materials.
// D3D9 front-face culling is expressed in final per-eye viewport pixels:
// screen Y points down, so a positive edge cross product is clockwise.
// Never infer world geometry or depth/cull source state from an FVF.
#include <cmath>
#include <cstdint>
namespace m2vr::vr {
enum class FaceCull : std::uint32_t { None=0, Clockwise=1, CounterClockwise=2 };
enum class DepthCompare : std::uint32_t { Less=0, LessEqual=1 };
struct RasterPolicy {
    FaceCull cull=FaceCull::None;
    DepthCompare depth=DepthCompare::Less;
    bool write_depth=true;
    // D3D9 COLORWRITEENABLE0: RGBA bits 1,2,4,8; default all.
    std::uint8_t write_rgba_mask=0x0fu;
};
inline bool ValidRasterPolicy(const RasterPolicy& p) noexcept {
    return (p.cull==FaceCull::None || p.cull==FaceCull::Clockwise ||
            p.cull==FaceCull::CounterClockwise) &&
           (p.depth==DepthCompare::Less ||
            p.depth==DepthCompare::LessEqual) &&
           (p.write_rgba_mask&0xf0u)==0u;
}
inline bool TriangleSurvivesCull(double area,FaceCull mode) noexcept {
    if(!std::isfinite(area)||std::abs(area)<1.e-8)return false;
    // Edge((x0,y0),(x1,y1),(x2,y2)) in a top-left-origin viewport:
    // CW screen-space winding has positive signed area.
    return mode==FaceCull::None ||
           (mode==FaceCull::Clockwise && area<0.) ||
           (mode==FaceCull::CounterClockwise && area>0.);
}
inline bool DepthPasses(double candidate,float existing,
                        DepthCompare func) noexcept {
    if(!std::isfinite(candidate) || !(candidate>0.) ||
       !ValidRasterPolicy({FaceCull::None,func,true}))return false;
    // The stereo Z buffer stores float, not double. Comparing a freshly
    // recomputed double against its previous float-rounded value can reject
    // the *same* projected triangle under LEQUAL when rounding goes down.
    // Quantize before both strict-less and equal-depth comparison, matching
    // the actual stored representation in both independent eyes.
    const float incoming=static_cast<float>(candidate);
    if(!std::isfinite(incoming))return false;
    return func==DepthCompare::Less ?
        incoming<existing : incoming<=existing;
}
} // namespace m2vr::vr
