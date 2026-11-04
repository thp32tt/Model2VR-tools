#pragma once
// Read-only original D3D9 A8 stage-zero alpha texture: the alpha channel
// is provided by the sole byte, RGB absent (zero). Alpha-only sources
// must not be incorrectly treated as white/opaque luminance pixels.
#include <d3d9.h>
#include <cstdint>
namespace m2vr::drawprobe {
inline constexpr std::uint32_t AlphaOnlyPixelBytes(D3DFORMAT f) noexcept {
    return f==D3DFMT_A8?1u:0u;
}
inline bool DecodeAlphaOnlyRowRGBA(
    const std::uint8_t* src,std::uint8_t* dst,
    std::uint32_t width,D3DFORMAT f) noexcept {
    if(!src || !dst || !AlphaOnlyPixelBytes(f) ||
       width==0u || width>1024u)return false;
    for(std::uint32_t x=0;x<width;++x){
        dst[x*4u]=0u;dst[x*4u+1u]=0u;dst[x*4u+2u]=0u;
        dst[x*4u+3u]=src[x];
    }
    return true;
}
} // namespace m2vr::drawprobe
