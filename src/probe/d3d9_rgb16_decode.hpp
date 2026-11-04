#pragma once
// Exact source D3D9 RGB16/A1R5G5B5/A4R4G4B4 color conversion.
// Used only for CPU-lockable real original textures in a separately verified
// world material pass; alpha is preserved and blending is not invented.
#include <d3d9.h>
#include <cstdint>
#include <cstddef>
namespace m2vr::drawprobe {
inline constexpr bool Supported16BitColor(D3DFORMAT format) noexcept {
    return format==D3DFMT_R5G6B5 || format==D3DFMT_X1R5G5B5 ||
           format==D3DFMT_A1R5G5B5 || format==D3DFMT_A4R4G4B4;
}
inline constexpr std::uint8_t Expand5(std::uint32_t v) noexcept {
    return static_cast<std::uint8_t>((v<<3u)|(v>>2u));
}
inline constexpr std::uint8_t Expand6(std::uint32_t v) noexcept {
    return static_cast<std::uint8_t>((v<<2u)|(v>>4u));
}
inline constexpr std::uint8_t Expand4(std::uint32_t v) noexcept {
    return static_cast<std::uint8_t>((v<<4u)|v);
}
inline bool Decode16BitColorRowRGBA(
    const std::uint8_t* row,std::uint8_t* rgba,
    std::uint32_t width,D3DFORMAT format) noexcept {
    if(!row || !rgba || !width || width>1024u ||
       !Supported16BitColor(format))return false;
    for(std::uint32_t x=0;x<width;++x){
        const std::uint16_t pixel=std::uint16_t(row[2u*x]) |
            (std::uint16_t(row[2u*x+1u])<<8u);
        std::uint8_t red=0,green=0,blue=0,alpha=255u;
        if(format==D3DFMT_A4R4G4B4){
            blue=Expand4(pixel&15u);
            green=Expand4((pixel>>4u)&15u);
            red=Expand4((pixel>>8u)&15u);
            alpha=Expand4((pixel>>12u)&15u);
        }else if(format==D3DFMT_R5G6B5){
            blue=Expand5(pixel&31u);
            green=Expand6((pixel>>5u)&63u);
            red=Expand5((pixel>>11u)&31u);
        }else{
            blue=Expand5(pixel&31u);
            green=Expand5((pixel>>5u)&31u);
            red=Expand5((pixel>>10u)&31u);
            if(format==D3DFMT_A1R5G5B5)
                alpha=(pixel&0x8000u)?255u:0u;
        }
        rgba[x*4u]=red;
        rgba[x*4u+1u]=green;
        rgba[x*4u+2u]=blue;
        rgba[x*4u+3u]=alpha;
    }
    return true;
}
} // namespace m2vr::drawprobe
