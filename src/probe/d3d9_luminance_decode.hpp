#pragma once
// Original CPU-lockable D3D9 luminance source formats.
// L8 has replicated RGB luminance and fully opaque alpha.
// A8L8 is little-endian (low byte luminance, high byte alpha).
// No readback, GPU state change or unsupported A8-only reinterpretation.
#include <d3d9.h>
#include <cstdint>

namespace m2vr::drawprobe {
inline constexpr std::uint32_t LuminancePixelBytes(D3DFORMAT f) noexcept {
    return (f==D3DFMT_L8 || f==D3DFMT_A4L4) ? 1u :
           f==D3DFMT_A8L8 ? 2u : 0u;
}
inline bool DecodeLuminanceRowRGBA(
    const std::uint8_t* input,std::uint8_t* output,
    std::uint32_t width,D3DFORMAT format) noexcept {
    const auto stride=LuminancePixelBytes(format);
    if(!input || !output || !stride || !width || width>1024u)return false;
    for(std::uint32_t x=0;x<width;++x){
        std::uint8_t luminance=input[x*stride];
        std::uint8_t alpha=stride==2u?input[x*stride+1u]:255u;
        if(format==D3DFMT_A4L4){
            const auto packed=luminance;
            // D3DFMT_A4L4 packs alpha in the upper nibble, luminance
            // in the lower nibble, both expanded losslessly to 8 bits.
            luminance=static_cast<std::uint8_t>(
                (packed&15u)*17u);
            alpha=static_cast<std::uint8_t>(
                ((packed>>4u)&15u)*17u);
        }
        output[4u*x+0u]=luminance;
        output[4u*x+1u]=luminance;
        output[4u*x+2u]=luminance;
        output[4u*x+3u]=alpha;
    }
    return true;
}
} // namespace m2vr::drawprobe
