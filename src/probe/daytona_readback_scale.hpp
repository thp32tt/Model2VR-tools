#pragma once
// CPU conversion of an independently rendered original D3D9 eye target to
// fixed 1024x1024 RGBA transport, retaining the native game's image aspect.
// Integer nearest-neighbor coordinates are calculated once per source size:
// no division or floating-point arithmetic in the per-frame pixel loop.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace m2vr::daytona {
// The original Sega Model 2 D3D9 backbuffer can be 16-bit even though
// stereo RGBA IPC must always be 32-bit. Do not assume 4 bytes per pixel.
enum class OriginalPacked16Layout : std::uint8_t {
    R5G6B5, X1R5G5B5, A1R5G5B5, A4R4G4B4
};
struct ReadbackScalePlan {
    static constexpr std::uint32_t kSide=1024u;
    static constexpr std::size_t kOutputBytes=
        std::size_t{kSide}*kSide*4u;
    std::uint32_t src_w=0u,src_h=0u;
    std::uint32_t dst_w=0u,dst_h=0u,left=0u,top=0u;
    std::array<std::uint16_t,kSide> sx{},sy{};
    bool Configure(std::uint32_t width,std::uint32_t height)noexcept{
        src_w=src_h=dst_w=dst_h=left=top=0u;
        if(!width||!height||width>2048u||height>2048u)return false;
        std::uint32_t w=kSide,h=kSide;
        if(width>=height){
            h=(std::max)(1u,static_cast<std::uint32_t>(
                std::uint64_t{height}*kSide/width));
        }else{
            w=(std::max)(1u,static_cast<std::uint32_t>(
                std::uint64_t{width}*kSide/height));
        }
        // Source dimensions are bounded and division by w/h is safe.
        for(std::uint32_t x=0;x<w;++x)
            sx[x]=static_cast<std::uint16_t>(
                (std::min)(width-1u,static_cast<std::uint32_t>(
                    std::uint64_t{x}*width/w)));
        for(std::uint32_t y=0;y<h;++y)
            sy[y]=static_cast<std::uint16_t>(
                (std::min)(height-1u,static_cast<std::uint32_t>(
                    std::uint64_t{y}*height/h)));
        src_w=width;src_h=height;dst_w=w;dst_h=h;
        left=(kSide-w)/2u;top=(kSide-h)/2u;
        return true;
    }
    bool ConvertPacked16(const std::uint8_t* pixels,int signed_pitch,
                         std::uint8_t* rgba,std::size_t bytes,
                         OriginalPacked16Layout layout)const noexcept{
        if(!src_w||!src_h||!dst_w||!dst_h||!pixels||!rgba||
           bytes!=kOutputBytes||signed_pitch<=0||
           std::uint64_t{static_cast<std::uint32_t>(signed_pitch)}<
               std::uint64_t{src_w}*2u)return false;
        const auto pitch=static_cast<std::size_t>(signed_pitch);
        std::memset(rgba,0,bytes);
        for(std::uint32_t y=0;y<dst_h;++y){
            const auto* row=pixels+static_cast<std::size_t>(sy[y])*pitch;
            auto* out=rgba+(std::size_t{top+y}*kSide+left)*4u;
            for(std::uint32_t x=0;x<dst_w;++x){
                std::uint16_t packed=0u;
                // A D3DLOCKED_RECT can have aligned or unaligned row pitch.
                std::memcpy(&packed,row+std::size_t{sx[x]}*2u,2u);
                std::uint8_t red=0u,green=0u,blue=0u;
                if(layout==OriginalPacked16Layout::R5G6B5){
                    const auto r=static_cast<std::uint8_t>((packed>>11u)&31u);
                    const auto g=static_cast<std::uint8_t>((packed>>5u)&63u);
                    const auto b=static_cast<std::uint8_t>(packed&31u);
                    red=static_cast<std::uint8_t>((r<<3u)|(r>>2u));
                    green=static_cast<std::uint8_t>((g<<2u)|(g>>4u));
                    blue=static_cast<std::uint8_t>((b<<3u)|(b>>2u));
                }else if(layout==OriginalPacked16Layout::X1R5G5B5 ||
                         layout==OriginalPacked16Layout::A1R5G5B5){
                    const auto r=static_cast<std::uint8_t>((packed>>10u)&31u);
                    const auto g=static_cast<std::uint8_t>((packed>>5u)&31u);
                    const auto b=static_cast<std::uint8_t>(packed&31u);
                    red=static_cast<std::uint8_t>((r<<3u)|(r>>2u));
                    green=static_cast<std::uint8_t>((g<<3u)|(g>>2u));
                    blue=static_cast<std::uint8_t>((b<<3u)|(b>>2u));
                }else if(layout==OriginalPacked16Layout::A4R4G4B4){
                    const auto r=static_cast<std::uint8_t>((packed>>8u)&15u);
                    const auto g=static_cast<std::uint8_t>((packed>>4u)&15u);
                    const auto b=static_cast<std::uint8_t>(packed&15u);
                    red=static_cast<std::uint8_t>((r<<4u)|r);
                    green=static_cast<std::uint8_t>((g<<4u)|g);
                    blue=static_cast<std::uint8_t>((b<<4u)|b);
                }else return false;
                // Backbuffer alpha is not display transparency: opaque eye.
                out[4u*x+0u]=red;
                out[4u*x+1u]=green;
                out[4u*x+2u]=blue;
                out[4u*x+3u]=255u;
            }
        }
        return true;
    }
    bool Convert(const std::uint8_t* bgra,int signed_pitch,
                 std::uint8_t* rgba,std::size_t bytes)const noexcept{
        if(!src_w||!src_h||!dst_w||!dst_h||!bgra||!rgba||
           bytes!=kOutputBytes||signed_pitch<=0||
           std::uint64_t{static_cast<std::uint32_t>(signed_pitch)}<
               std::uint64_t{src_w}*4u)return false;
        const auto pitch=static_cast<std::size_t>(signed_pitch);
        std::memset(rgba,0,bytes);
        for(std::uint32_t y=0;y<dst_h;++y){
            const auto* row=bgra+static_cast<std::size_t>(sy[y])*pitch;
            auto* out=rgba+(std::size_t{top+y}*kSide+left)*4u;
            for(std::uint32_t x=0;x<dst_w;++x){
                const auto* p=row+std::size_t{sx[x]}*4u;
                out[4u*x+0u]=p[2];
                out[4u*x+1u]=p[1];
                out[4u*x+2u]=p[0];
                out[4u*x+3u]=255u;
            }
        }
        return true;
    }
};
} // namespace m2vr::daytona
