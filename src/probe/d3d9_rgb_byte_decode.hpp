#pragma once
// D3D9 R3G3B2 / R8G8B8 CPU-lockable texture bytes to RGBA8.
// These are original source pixels, not a shader/world ownership inference.
#include <d3d9.h>
#include <cstdint>
namespace m2vr::drawprobe {
inline constexpr std::uint32_t RgbBytePixelBytes(D3DFORMAT f) noexcept {
    return f==D3DFMT_R3G3B2?1u:f==D3DFMT_R8G8B8?3u:0u;
}
inline constexpr std::uint8_t Expand3(std::uint32_t v) noexcept {
    return static_cast<std::uint8_t>((v<<5u)|(v<<2u)|(v>>1u));
}
inline bool DecodeRgbByteRowRGBA(
    const std::uint8_t* src,std::uint8_t* dst,
    std::uint32_t width,D3DFORMAT f) noexcept {
    if(!src || !dst || !RgbBytePixelBytes(f) ||
       width==0u || width>1024u)return false;
    for(std::uint32_t i=0;i<width;++i){
        std::uint8_t red=0,green=0,blue=0;
        if(f==D3DFMT_R8G8B8){
            blue=src[3u*i];green=src[3u*i+1u];red=src[3u*i+2u];
        }else{
            const auto x=src[i];
            red=Expand3((x>>5u)&7u);
            green=Expand3((x>>2u)&7u);
            blue=static_cast<std::uint8_t>((x&3u)*85u);
        }
        dst[4u*i]=red;dst[4u*i+1u]=green;
        dst[4u*i+2u]=blue;dst[4u*i+3u]=255u;
    }
    return true;
}
} // namespace m2vr::drawprobe
