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
    static_assert(NeedsRefresh(41u,40u,{false,0,0},{false,0,0}));
    static_assert(NeedsRefresh(41u,41u,{true,1,2},{true,2,2}));
    static_assert(NeedsRefresh(41u,41u,{true,1,2},{false,1,2}));
    static_assert(!NeedsRefresh(41u,41u,{true,1,2},{true,1,2}));
    static_assert(!NeedsRefresh(41u,41u,{false,1,2},{false,3,4}));
    static_assert(!NeedsRefresh(0u,0u,{true,1,2},{false,0,0}));
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
    // XR-rate reticle movement must match FULL raw screenshot re-composition
    // byte for byte while touching only the old crosshair footprint.
    std::array<std::uint8_t,w*h*4u+2u> incremental=base;
    State previous{};
    constexpr std::array<State,12> sequence{{
        {true,16,12}, {true,8,12}, {true,17,13},
        {true,17,13}, {false,777,888}, {true,0,0},
        {true,w-1u,h-1u}, {true,1,h-2u},
        {false,0,0}, {true,31,0}, {true,12,23}, {false,0,0}
    }};
    for(const auto next:sequence){
        if(previous.visible)
            assert(RestoreRgba(incremental.data(),w*h*4u,
                               base.data(),w*h*4u,w,h,previous));
        assert(PaintRgba(incremental.data(),w*h*4u,w,h,next));
        auto full_copy=base;
        assert(PaintRgba(full_copy.data(),w*h*4u,w,h,next));
        assert(incremental==full_copy);
        assert(base[pos(16,12)]==17u);
        previous=next;
    }
    assert(!RestoreRgba(nullptr,w*h*4u,base.data(),w*h*4u,
                        w,h,{true,1,1}));
    assert(!RestoreRgba(incremental.data(),w*h*4u,nullptr,w*h*4u,
                        w,h,{true,1,1}));
    assert(!RestoreRgba(incremental.data(),w*h*4u-1u,
                        base.data(),w*h*4u,w,h,{true,1,1}));
    assert(!RestoreRgba(incremental.data(),w*h*4u,
                        base.data(),w*h*4u-1u,w,h,{true,1,1}));
    assert(!RestoreRgba(incremental.data(),w*h*4u,
                        base.data(),w*h*4u,w,h,{true,w,1}));
    assert(!RestoreRgba(incremental.data(),w*h*4u,
                        base.data(),w*h*4u,w,h,{true,1,h}));

    // XR round-robin image caches may be several reticle poses behind.
    constexpr UploadSlot prior{48u,{true,16,12},true};
    constexpr auto near=PlanUpload(prior,48u,{true,8,12},1024u,1024u);
    static_assert(near.partial && near.dirty.left==0u);
    static_assert(near.dirty.top==2u && near.dirty.right==27u);
    static_assert(near.dirty.bottom==23u && near.dirty.Area()==567u);
    static_assert(!PlanUpload(prior,49u,{true,8,12},1024u,1024u).partial);
    static_assert(!PlanUpload({},48u,{true,8,12},1024u,1024u).partial);
    static_assert(!PlanUpload(prior,0u,{true,8,12},1024u,1024u).partial);
    constexpr UploadSlot edge{55u,{true,0u,0u},true};
    constexpr auto small=PlanUpload(edge,55u,{true,2u,2u},1024u,1024u);
    static_assert(small.partial && small.dirty.left==0u);
    static_assert(small.dirty.right==13u && small.dirty.bottom==13u);
    static_assert(!PlanUpload(edge,55u,{true,1023u,1023u},
                              1024u,1024u).partial);
    // A long controller sweep previously caused a FULL 4MiB upload;
    // old/new strokes are disjoint and should use TWO small D3D11 boxes.
    constexpr UploadSlot far_prior{65u,{true,100u,100u},true};
    constexpr auto far=PlanUpload(far_prior,65u,{true,800u,800u},
                                  1024u,1024u);
    static_assert(far.partial && far.two_boxes);
    static_assert(far.dirty.left==90u && far.dirty.top==90u);
    static_assert(far.dirty.right==111u && far.dirty.bottom==111u);
    static_assert(far.second.left==790u && far.second.top==790u);
    static_assert(far.second.right==811u && far.second.bottom==811u);
    static_assert(far.dirty.Area()+far.second.Area()==882u);
    static_assert(!near.two_boxes && !small.two_boxes);
    static_assert(RectsDisjoint(far.dirty,far.second));
    static_assert(!RectsDisjoint(far.dirty,far.dirty));
    static_assert(!PlanUpload(far_prior,66u,{true,800u,800u},
                              1024u,1024u).partial);
    // New or invalid screenshot may NEVER reuse the old eye-box contents.
    static_assert(!PlanUpload(far_prior,0u,{true,800u,800u},
                              1024u,1024u).two_boxes);
    constexpr auto hide=PlanUpload(edge,55u,{},1024u,1024u);
    static_assert(hide.partial && hide.dirty.Area()==121u);
    static_assert(!PlanUpload(
        UploadSlot{44u,{},true},44u,{},1024u,1024u).partial);
    static_assert(!PlanUpload(
        UploadSlot{44u,{true,1u,1u},false},44u,{true,2u,2u},
        1024u,1024u).partial);
    return 0;
}
