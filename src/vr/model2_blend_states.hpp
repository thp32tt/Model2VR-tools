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
    SourceAlphaAdd=3, PremultipliedOver=4, ReplaceRGBA=5,
    // D3D9 non-separate fixed-function ADD: identical source/destination
    // factors for the original RGB and alpha channels.
    MultiplyColor=6, DestinationAlphaOver=7, PreserveDestination=8
};
inline bool BlendProducesColor(BlendMode mode) noexcept {
    return mode!=BlendMode::PreserveDestination;
}
inline bool ValidBlendMode(BlendMode mode) noexcept {
    return mode==BlendMode::Opaque || mode==BlendMode::SourceAlphaOver ||
           mode==BlendMode::Additive || mode==BlendMode::SourceAlphaAdd ||
           mode==BlendMode::PremultipliedOver ||
           mode==BlendMode::ReplaceRGBA ||
           mode==BlendMode::MultiplyColor ||
           mode==BlendMode::DestinationAlphaOver ||
           mode==BlendMode::PreserveDestination;
}
// Without D3DRS_SEPARATEALPHABLENDENABLE, RGB and alpha share factor
// pairs; fixed-function D3DBLENDOP_ADD saturates all four destination bytes.
inline void CompositeSourceRGBA(
    std::uint8_t* dst,const std::array<std::uint8_t,4>& source,
    BlendMode mode,std::uint8_t write_mask=0x0fu) noexcept {
    if(write_mask&0xf0u)return;
    if(mode==BlendMode::Opaque){
        // D3DRS_ALPHABLENDENABLE=FALSE disables blending; it does NOT
        // discard or force the pixel's alpha. When all COLORWRITE channels
        // are enabled the original source RGBA replaces destination RGBA.
        // A later D3D9 blend must see this actual 8-bit destination alpha.
        for(std::size_t i=0;i<4;++i)
            if(write_mask&(1u<<i))dst[i]=source[i];
        return;
    }
    if(mode==BlendMode::ReplaceRGBA){
        // ALPHABLENDENABLE=TRUE, ONE/ZERO replaces *all four* D3D9
        // destination channels. Unlike disabled blending it must keep
        // the incoming source alpha for subsequent world material passes.
        for(std::size_t i=0;i<4;++i)
            if(write_mask&(1u<<i))dst[i]=source[i];
        return;
    }
    if(mode==BlendMode::PreserveDestination)return;
    const double alpha=double(source[3])/255.;
    // Destination alpha MUST be captured before the first masked write.
    // D3DBLEND_DESTALPHA/INVDESTALPHA uses the original destination A
    // for every RGB/A channel, even when alpha is itself writable.
    const double destination_alpha=double(dst[3])/255.;
    const double sf=mode==BlendMode::DestinationAlphaOver ?
                         destination_alpha :
                     (mode==BlendMode::Additive ||
                      mode==BlendMode::PremultipliedOver)?1.:alpha;
    const double df=mode==BlendMode::DestinationAlphaOver ?
                         1.-destination_alpha :
                     (mode==BlendMode::SourceAlphaOver ||
                      mode==BlendMode::PremultipliedOver)?1.-alpha:1.;
    for(std::size_t i=0;i<4;++i) {
        if(!(write_mask&(1u<<i)))continue;
        // DESTCOLOR/ZERO and ZERO/SRCCOLOR are the same commutative
        // per-channel multiplication (also for source/destination alpha).
        const double value=mode==BlendMode::MultiplyColor ?
            double(source[i])*double(dst[i])/255. :
            double(source[i])*sf+double(dst[i])*df;
        dst[i]=static_cast<std::uint8_t>(std::lround(std::clamp(
            value,0.,255.)));
    }
}
} // namespace m2vr::vr
