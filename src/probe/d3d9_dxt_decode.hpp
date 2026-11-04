#pragma once
// Decode *CPU-lockable* original D3D9 DXT1/DXT3/DXT5 blocks for the
// separately authenticated world material bridge. No GPU readback, shader
// replacement, palette approximation or game resource mutation.
#include <d3d9.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace m2vr::drawprobe {
inline constexpr std::uint32_t DxtBytesPerBlock(D3DFORMAT format) noexcept {
    switch(format){
      case D3DFMT_DXT1:return 8u;
      case D3DFMT_DXT3:case D3DFMT_DXT5:return 16u;
      default:return 0u;
    }
}
struct DxtRGBA {
    std::uint8_t r=0,g=0,b=0,a=255;
};
inline DxtRGBA Decode565(std::uint16_t value) noexcept {
    const std::uint32_t r=(value>>11u)&31u;
    const std::uint32_t g=(value>>5u)&63u;
    const std::uint32_t b=value&31u;
    return {static_cast<std::uint8_t>((r<<3u)|(r>>2u)),
            static_cast<std::uint8_t>((g<<2u)|(g>>4u)),
            static_cast<std::uint8_t>((b<<3u)|(b>>2u)),255u};
}
inline DxtRGBA InterpolateColor(const DxtRGBA& a,const DxtRGBA& b,
                                 unsigned wa,unsigned wb,
                                 unsigned divisor) noexcept {
    return {static_cast<std::uint8_t>((wa*a.r+wb*b.r)/divisor),
            static_cast<std::uint8_t>((wa*a.g+wb*b.g)/divisor),
            static_cast<std::uint8_t>((wa*a.b+wb*b.b)/divisor),255u};
}
// The caller has already guarded a complete 8/16-byte compressed block
// copied from a source LockRect. Destination is a tightly-packed RGBA8
// buffer with checked width, height, and >=width*4 row pitch.
inline bool DecodeDxtBlockRGBA(const std::uint8_t* block,
        D3DFORMAT format,std::uint32_t block_x,std::uint32_t block_y,
        std::uint8_t* rgba,std::uint32_t width,std::uint32_t height,
        std::uint32_t output_pitch) noexcept {
    const auto block_bytes=DxtBytesPerBlock(format);
    if(!block || !rgba || !block_bytes || !width || !height ||
       width>1024u || height>1024u || output_pitch<width*4u ||
       block_x >= (width+3u)/4u || block_y >= (height+3u)/4u)return false;
    const std::uint8_t* colors=block+(block_bytes==8u?0u:8u);
    const std::uint16_t c0=std::uint16_t(colors[0]) |
                            std::uint16_t(colors[1])<<8u;
    const std::uint16_t c1=std::uint16_t(colors[2]) |
                            std::uint16_t(colors[3])<<8u;
    std::array<DxtRGBA,4> palette{};
    palette[0]=Decode565(c0);
    palette[1]=Decode565(c1);
    if(c0>c1 || block_bytes==16u){
        palette[2]=InterpolateColor(palette[0],palette[1],2,1,3);
        palette[3]=InterpolateColor(palette[0],palette[1],1,2,3);
    }else{
        palette[2]=InterpolateColor(palette[0],palette[1],1,1,2);
        palette[3]={0u,0u,0u,0u};
    }
    std::uint32_t color_bits=0u;
    for(unsigned i=0;i<4u;++i)
        color_bits|=std::uint32_t(colors[4u+i])<<(8u*i);
    std::uint64_t alpha_bits=0u;
    std::array<std::uint8_t,8> alpha{};
    if(format==D3DFMT_DXT5){
        const std::uint32_t a0=block[0],a1=block[1];
        alpha[0]=static_cast<std::uint8_t>(a0);
        alpha[1]=static_cast<std::uint8_t>(a1);
        if(a0>a1){
            for(unsigned i=2u;i<8u;++i)
                alpha[i]=static_cast<std::uint8_t>(
                    ((8u-i)*a0+(i-1u)*a1)/7u);
        }else{
            for(unsigned i=2u;i<6u;++i)
                alpha[i]=static_cast<std::uint8_t>(
                    ((6u-i)*a0+(i-1u)*a1)/5u);
            alpha[6]=0u;
            alpha[7]=255u;
        }
        for(unsigned i=0;i<6u;++i)
            alpha_bits|=std::uint64_t(block[2u+i])<<(8u*i);
    }else if(format==D3DFMT_DXT3){
        for(unsigned i=0;i<8u;++i)
            alpha_bits|=std::uint64_t(block[i])<<(8u*i);
    }
    for(unsigned y=0;y<4u;++y){
        const auto destination_y=block_y*4u+y;
        if(destination_y>=height)continue;
        for(unsigned x=0;x<4u;++x){
            const auto destination_x=block_x*4u+x;
            if(destination_x>=width)continue;
            const unsigned pixel=y*4u+x;
            const unsigned index=(color_bits>>(pixel*2u))&3u;
            auto color=palette[index];
            if(format==D3DFMT_DXT3)
                color.a=static_cast<std::uint8_t>(
                    ((alpha_bits>>(pixel*4u))&15u)*17u);
            else if(format==D3DFMT_DXT5)
                color.a=alpha[(alpha_bits>>(pixel*3u))&7u];
            auto* dst=rgba+std::size_t(destination_y)*output_pitch+
                      std::size_t(destination_x)*4u;
            dst[0]=color.r;dst[1]=color.g;dst[2]=color.b;dst[3]=color.a;
        }
    }
    return true;
}
} // namespace m2vr::drawprobe
