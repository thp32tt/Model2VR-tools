#pragma once
// Exact D3D9 fixed-function source RGBA blending into verified stereo eyes.
// No generic Draw is classified as gameplay world by this renderer.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace m2vr::vr {
enum class BlendMode : std::uint32_t {
    Opaque=0, SourceAlphaOver=1, Additive=2,
    SourceAlphaAdd=3, PremultipliedOver=4, ReplaceRGBA=5
};
inline bool ValidBlendMode(BlendMode mode) noexcept {
    return mode==BlendMode::Opaque || mode==BlendMode::SourceAlphaOver ||
           mode==BlendMode::Additive || mode==BlendMode::SourceAlphaAdd ||
           mode==BlendMode::PremultipliedOver ||
           mode==BlendMode::ReplaceRGBA;
}
// Without D3DRS_SEPARATEALPHABLENDENABLE, RGB and alpha share factor
// pairs; fixed-function D3DBLENDOP_ADD saturates all four destination bytes.
inline void CompositeSourceRGBA(
    std::uint8_t* dst,const std::array<std::uint8_t,4>& source,
    BlendMode mode) noexcept {
    if(mode==BlendMode::Opaque){
        for(std::size_t i=0;i<3;++i)dst[i]=source[i];
        dst[3]=255u; // preserve pre-existing opaque world raster ABI
        return;
    }
    if(mode==BlendMode::ReplaceRGBA){
        // ALPHABLENDENABLE=TRUE, ONE/ZERO replaces *all four* D3D9
        // destination channels. Unlike disabled blending it must keep
        // the incoming source alpha for subsequent world material passes.
        for(std::size_t i=0;i<4;++i)dst[i]=source[i];
        return;
    }
    const double alpha=double(source[3])/255.;
    const double sf=(mode==BlendMode::Additive ||
                     mode==BlendMode::PremultipliedOver)?1.:alpha;
    const double df=(mode==BlendMode::SourceAlphaOver ||
                     mode==BlendMode::PremultipliedOver)?1.-alpha:1.;
    for(std::size_t i=0;i<4;++i)
        dst[i]=static_cast<std::uint8_t>(std::lround(std::clamp(
            double(source[i])*sf+double(dst[i])*df,0.,255.)));
}
} // namespace m2vr::vr
