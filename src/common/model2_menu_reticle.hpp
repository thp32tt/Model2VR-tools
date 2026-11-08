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
// A new GDI screenshot OR a changed live reticle must get a new XR image
// upload token. Unchanged reticle state reuses the existing GPU image; while
// hidden, stale X/Y coordinates never create phantom visual updates.
constexpr bool NeedsRefresh(std::uint64_t capture_serial,
                            std::uint64_t prepared_serial,
                            State now,State prepared) noexcept {
    return capture_serial!=0u &&
           (capture_serial!=prepared_serial || !Same(now,prepared));
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

// Recover just the previous crosshair's affected pixels from the immutable
// raw screenshot. An XR-cadence pointer move must not copy the entire 4MiB
// desktop image again; only a genuinely new 10fps screenshot needs that.
// Re-applying a new reticle after this operation is pixel-identical to
// painting into a fresh copy of the raw screenshot.
inline bool RestoreRgba(std::uint8_t* pixels,std::size_t bytes,
                        const std::uint8_t* source,std::size_t source_bytes,
                        std::uint32_t width,std::uint32_t height,
                        State previous) noexcept {
    if(!pixels || !source || !width || !height ||
       width>8192u || height>8192u ||
       std::uint64_t(width)*height*4u>bytes ||
       std::uint64_t(width)*height*4u>source_bytes)return false;
    if(!previous.visible)return true;
    if(previous.x>=width || previous.y>=height)return false;
    const auto recover=[&](std::int64_t x,std::int64_t y){
        if(x<0 || y<0 ||
           x>=static_cast<std::int64_t>(width) ||
           y>=static_cast<std::int64_t>(height))return;
        const auto at=(std::size_t(y)*width+std::size_t(x))*4u;
        for(std::size_t channel=0;channel<4u;++channel)
            pixels[at+channel]=source[at+channel];
    };
    constexpr std::int64_t kRadius=10;
    for(std::int64_t d=-kRadius;d<=kRadius;++d)
        for(std::int64_t t=-1;t<=1;++t){
            recover(std::int64_t(previous.x)+d,std::int64_t(previous.y)+t);
            recover(std::int64_t(previous.x)+t,std::int64_t(previous.y)+d);
        }
    return true;
}

struct Rect {
    std::uint32_t left=0,top=0,right=0,bottom=0;
    constexpr bool Valid() const noexcept{return left<right && top<bottom;}
    constexpr std::uint64_t Area() const noexcept{
        return Valid()?std::uint64_t(right-left)*(bottom-top):0u;
    }
};
// Bounds conservatively cover the previous/current 21x21 cyan crosshair.
constexpr Rect ReticleBounds(State s,std::uint32_t w,std::uint32_t h) noexcept {
    if(!s.visible || !w || !h || w>8192u || h>8192u ||
       s.x>=w || s.y>=h)return {};
    constexpr std::uint32_t r=10u;
    return {s.x>r?s.x-r:0u,s.y>r?s.y-r:0u,
            s.x+r+1u<w?s.x+r+1u:w,
            s.y+r+1u<h?s.y+r+1u:h};
}
constexpr Rect ChangedBounds(State before,State after,
                             std::uint32_t w,std::uint32_t h) noexcept {
    const auto a=ReticleBounds(before,w,h);
    const auto b=ReticleBounds(after,w,h);
    if(!a.Valid())return b;
    if(!b.Valid())return a;
    return {a.left<b.left?a.left:b.left,
            a.top<b.top?a.top:b.top,
            a.right>b.right?a.right:b.right,
            a.bottom>b.bottom?a.bottom:b.bottom};
}
// State is per actual acquired XR swapchain image, not global. Game pixels,
// new screenshots, cleared images and invalid sessions never qualify.
struct UploadSlot {
    std::uint64_t capture_serial=0u;
    State reticle{};
    bool valid=false;
};
struct UploadPlan {
    Rect dirty{};
    bool partial=false;
};
constexpr UploadPlan PlanUpload(UploadSlot prior,
                                 std::uint64_t captured,State current,
                                 std::uint32_t w,std::uint32_t h) noexcept {
    if(!prior.valid || !captured || prior.capture_serial!=captured ||
       !w || !h || w>8192u || h>8192u)return {};
    const auto dirty=ChangedBounds(prior.reticle,current,w,h);
    // Avoid huge unions on far pointer moves: use safe full upload instead.
    if(!dirty.Valid() || dirty.Area()>std::uint64_t(w)*h/4u)return {};
    return {dirty,true};
}
} // namespace m2vr::menureticle
