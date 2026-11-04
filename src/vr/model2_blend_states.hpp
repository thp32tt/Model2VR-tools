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
// D3D9 separate-alpha factors operate on the ALPHA channel only.
// SRCCOLOR==SRCALPHA and DESTCOLOR==DESTALPHA for that one component;
// other D3DBLEND values are never approximated by this source renderer.
enum class AlphaBlendFactor : std::uint8_t {
    Zero=0, One=1, SourceAlpha=2, InverseSourceAlpha=3,
    DestinationAlpha=4, InverseDestinationAlpha=5
};
struct SeparateAlphaBlend {
    bool enabled=false;
    AlphaBlendFactor source=AlphaBlendFactor::One;
    AlphaBlendFactor destination=AlphaBlendFactor::Zero;
};
inline constexpr bool ValidSeparateAlphaBlend(
    const SeparateAlphaBlend& policy) noexcept {
    if(!policy.enabled)
        return policy.source==AlphaBlendFactor::One &&
               policy.destination==AlphaBlendFactor::Zero;
    return static_cast<unsigned>(policy.source)<=5u &&
           static_cast<unsigned>(policy.destination)<=5u;
}
inline double SeparateAlphaFactorValue(
    AlphaBlendFactor factor,double source_alpha,
    double destination_alpha) noexcept {
    switch(factor) {
    case AlphaBlendFactor::Zero:return 0.;
    case AlphaBlendFactor::One:return 1.;
    case AlphaBlendFactor::SourceAlpha:return source_alpha;
    case AlphaBlendFactor::InverseSourceAlpha:return 1.-source_alpha;
    case AlphaBlendFactor::DestinationAlpha:return destination_alpha;
    case AlphaBlendFactor::InverseDestinationAlpha:return 1.-destination_alpha;
    }
    return 0.;
}
// Without D3DRS_SEPARATEALPHABLENDENABLE, RGB and alpha share factor
// pairs; fixed-function D3DBLENDOP_ADD saturates all four destination bytes.
inline void CompositeSourceRGBA(
    std::uint8_t* dst,const std::array<std::uint8_t,4>& source,
    BlendMode mode,std::uint8_t write_mask=0x0fu,
    const SeparateAlphaBlend& separate={}) noexcept {
    if((write_mask&0xf0u) || !ValidBlendMode(mode) ||
       !ValidSeparateAlphaBlend(separate) ||
       (separate.enabled && mode==BlendMode::Opaque))return;
    if(mode==BlendMode::Opaque){
        // D3DRS_ALPHABLENDENABLE=FALSE disables blending; it does NOT
        // discard or force the pixel's alpha. When all COLORWRITE channels
        // are enabled the original source RGBA replaces destination RGBA.
        // A later D3D9 blend must see this actual 8-bit destination alpha.
        for(std::size_t i=0;i<4;++i)
            if(write_mask&(1u<<i))dst[i]=source[i];
        return;
    }
    if(mode==BlendMode::ReplaceRGBA && !separate.enabled){
        // ALPHABLENDENABLE=TRUE, ONE/ZERO replaces *all four* D3D9
        // destination channels. Unlike disabled blending it must keep
        // the incoming source alpha for subsequent world material passes.
        for(std::size_t i=0;i<4;++i)
            if(write_mask&(1u<<i))dst[i]=source[i];
        return;
    }
    if(mode==BlendMode::PreserveDestination && !separate.enabled)return;
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
        if(i==3u && separate.enabled) {
            // D3D9 separate alpha factors use the DESTINATION alpha
            // sampled BEFORE any RGB write, with an independent ADD.
            const double source_factor=SeparateAlphaFactorValue(
                separate.source,alpha,destination_alpha);
            const double destination_factor=SeparateAlphaFactorValue(
                separate.destination,alpha,destination_alpha);
            const double value=double(source[3])*source_factor+
                               double(dst[3])*destination_factor;
            dst[3]=static_cast<std::uint8_t>(std::lround(
                std::clamp(value,0.,255.)));
            continue;
        }
        if(mode==BlendMode::ReplaceRGBA) {
            dst[i]=source[i];
            continue;
        }
        if(mode==BlendMode::PreserveDestination)continue;
        // DESTCOLOR/ZERO and ZERO/SRCCOLOR are commutative multiplication
        // for every non-separate channel, including the legacy alpha path.
        const double value=mode==BlendMode::MultiplyColor ?
            double(source[i])*double(dst[i])/255. :
            double(source[i])*sf+double(dst[i])*df;
        dst[i]=static_cast<std::uint8_t>(std::lround(std::clamp(
            value,0.,255.)));
    }
}
} // namespace m2vr::vr
