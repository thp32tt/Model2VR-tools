#pragma once
// Bounded software texture sampling for verified Model 2 stereo geometry.
// D3D9 normalized coordinates: POINT uses floor(u * size); LINEAR samples
// around texel centers at u * size - 0.5. Coordinates can clamp or wrap
// independently per axis, including mirrored repeat. No mip/anisotropic,
// border or blending emulation.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace m2vr::vr {
enum class TextureAddress : std::uint32_t {
    Clamp=0u, Wrap=1u, Mirror=2u
};
enum class TextureFilter : std::uint32_t { Point=0u, Linear=1u };
struct SamplerPolicy {
    TextureAddress u=TextureAddress::Clamp;
    TextureAddress v=TextureAddress::Clamp;
    TextureFilter filter=TextureFilter::Point;
};
struct SampledRGBA {
    std::uint8_t r=0,g=0,b=0,a=0;
};
inline bool ValidSampler(SamplerPolicy policy) noexcept {
    const auto address_ok=[](TextureAddress x){
        return x==TextureAddress::Clamp || x==TextureAddress::Wrap ||
               x==TextureAddress::Mirror;
    };
    return address_ok(policy.u) && address_ok(policy.v) &&
        (policy.filter==TextureFilter::Point ||
         policy.filter==TextureFilter::Linear);
}
inline std::uint32_t MapTexel(std::int64_t n,std::uint32_t size,
                              TextureAddress address) noexcept {
    if(address==TextureAddress::Clamp){
        if(n<0)return 0;
        if(n>=static_cast<std::int64_t>(size))return size-1;
        return static_cast<std::uint32_t>(n);
    }
    const auto m=static_cast<std::int64_t>(size);
    if(address==TextureAddress::Mirror){
        // Reflect around both texel-edge boundaries. The mirrored
        // 2*size period must work for negative indices too, so bilinear
        // footprints do not sample the opposite edge at a reflection seam.
        const auto period=m*2;
        const auto wrapped=((n%period)+period)%period;
        return static_cast<std::uint32_t>(
            wrapped<m ? wrapped : (period-1-wrapped));
    }
    return static_cast<std::uint32_t>(((n%m)+m)%m);
}
inline bool SampleTextureRGBA(const std::uint8_t* pixels,
                              std::uint32_t width,std::uint32_t height,
                              std::uint32_t pitch,
                              double u,double v,
                              SamplerPolicy policy,
                              SampledRGBA& out) noexcept {
    if(!pixels || !width || !height || width>1024 || height>1024 ||
       pitch!=width*4u || !std::isfinite(u) || !std::isfinite(v) ||
       std::abs(u)>65536. || std::abs(v)>65536. ||
       !ValidSampler(policy))return false;
    const auto texel=[&](std::int64_t x,std::int64_t y,
                          std::uint32_t channel)->std::uint8_t {
        const auto sx=MapTexel(x,width,policy.u);
        const auto sy=MapTexel(y,height,policy.v);
        return pixels[std::size_t(sy)*pitch+std::size_t(sx)*4u+channel];
    };
    if(policy.filter==TextureFilter::Point){
        const auto x=static_cast<std::int64_t>(std::floor(u*width));
        const auto y=static_cast<std::int64_t>(std::floor(v*height));
        out={texel(x,y,0),texel(x,y,1),texel(x,y,2),texel(x,y,3)};
        return true;
    }
    const double tx=u*width-0.5,ty=v*height-0.5;
    const auto x=static_cast<std::int64_t>(std::floor(tx));
    const auto y=static_cast<std::int64_t>(std::floor(ty));
    const double fx=tx-x,fy=ty-y;
    std::uint8_t result[4]{};
    for(std::uint32_t c=0;c<4;++c){
        const double a=texel(x,y,c)*(1.-fx)+texel(x+1,y,c)*fx;
        const double b=texel(x,y+1,c)*(1.-fx)+texel(x+1,y+1,c)*fx;
        const double mix=a*(1.-fy)+b*fy;
        result[c]=static_cast<std::uint8_t>(
            (std::min)(255.,(std::max)(0.,std::floor(mix+0.5))));
    }
    out={result[0],result[1],result[2],result[3]};
    return true;
}
} // namespace m2vr::vr
