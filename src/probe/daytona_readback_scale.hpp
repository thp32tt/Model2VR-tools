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
