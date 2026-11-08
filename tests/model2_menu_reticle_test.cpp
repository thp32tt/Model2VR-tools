#include "../src/common/model2_menu_reticle.hpp"
#include <array>
#include <cassert>
#include <cstdint>
using namespace m2vr::menureticle;
int main(){
    static_assert(Same({},{}));
    static_assert(Same({false,1,2},{false,999,888}));
    static_assert(Same({true,10,12},{true,10,12}));
    static_assert(!Same({true,10,12},{true,11,12}));
    static_assert(!Same({true,10,12},{false,10,12}));
    // Source BGRA GDI bytes are already converted to opaque RGBA. Reticle
    // must have no effect on the underlying immutable screenshot pixels.
    constexpr std::uint32_t w=32,h=24;
    std::array<std::uint8_t,w*h*4u+2u> base{};
    base.fill(17u);
    base[0u]=33u;
    auto prepared=base;
    assert(PaintRgba(prepared.data(),w*h*4u,w,h,{true,16,12}));
    const auto pos=[&](std::uint32_t x,std::uint32_t y){
        return (std::size_t(y)*w+x)*4u;
    };
    assert(prepared[pos(16,12)+0u]==0u);
    assert(prepared[pos(16,12)+1u]==255u);
    assert(prepared[pos(16,12)+2u]==255u);
    assert(prepared[pos(16,12)+3u]==255u);
    assert(prepared[pos(0,0)]==33u);
    assert(prepared[w*h*4u]==17u && prepared[w*h*4u+1u]==17u);
    assert(base[pos(16,12)]==17u);
    // Next XR reticle position uses an unmodified base snapshot, so the
    // old 10fps crosshair cannot remain visible as a ghost.
    auto moved=base;
    assert(PaintRgba(moved.data(),w*h*4u,w,h,{true,8,12}));
    assert(moved[pos(16,20)+1u]==17u); // old vertical stroke is erased
    assert(moved[pos(8,12)+1u]==255u);
    // Bounds/empty/invalid input are safe and fail closed.
    auto clipped=base;
    assert(PaintRgba(clipped.data(),w*h*4u,w,h,{true,0,0}));
    assert(clipped[w*h*4u]==17u && clipped[w*h*4u+1u]==17u);
    assert(!PaintRgba(clipped.data(),w*h*4u-1u,w,h,{true,1,1}));
    assert(!PaintRgba(nullptr,w*h*4u,w,h,{true,1,1}));
    assert(!PaintRgba(clipped.data(),w*h*4u,w,h,{true,w,1}));
    assert(!PaintRgba(clipped.data(),w*h*4u,w,h,{true,1,h}));
    auto no_hover=base;
    assert(PaintRgba(no_hover.data(),w*h*4u,w,h,{false,0,0}));
    assert(no_hover==base);
    return 0;
}
