#pragma once
// Visual-only RGBA overlay, separate from the 10fps authenticated Windows
// screenshot and all original D3D9/FFB/CPU-World3D source evidence.
#include <cstddef>
#include <cstdint>
namespace m2vr::menureticle {
struct State {
    bool visible=false;
    std::uint32_t x=0,y=0;
};
constexpr bool Same(State a,State b) noexcept {
    return a.visible==b.visible &&
           (!a.visible || (a.x==b.x && a.y==b.y));
}
// Bright cyan, opaque 2D crosshair. Writes only inside the provided RGBA
// image; never touches Windows input, OpenXR tracking or game pixels.
inline bool PaintRgba(std::uint8_t* pixels,std::size_t bytes,
                      std::uint32_t width,std::uint32_t height,
                      State state) noexcept {
    if(!pixels || !width || !height ||
       width>8192u || height>8192u ||
       std::uint64_t(width)*height*4u>bytes)return false;
    if(!state.visible)return true;
    if(state.x>=width || state.y>=height)return false;
    const auto paint=[&](std::int64_t x,std::int64_t y){
        if(x<0 || y<0 ||
           x>=static_cast<std::int64_t>(width) ||
           y>=static_cast<std::int64_t>(height))return;
        const auto at=(std::size_t(y)*width+std::size_t(x))*4u;
        pixels[at+0u]=0u; // R
        pixels[at+1u]=255u; // G
        pixels[at+2u]=255u; // B
        pixels[at+3u]=255u; // A
    };
    constexpr std::int64_t kRadius=10;
    for(std::int64_t d=-kRadius;d<=kRadius;++d)
        for(std::int64_t t=-1;t<=1;++t){
            paint(std::int64_t(state.x)+d,std::int64_t(state.y)+t);
            paint(std::int64_t(state.x)+t,std::int64_t(state.y)+d);
        }
    return true;
}
} // namespace m2vr::menureticle
