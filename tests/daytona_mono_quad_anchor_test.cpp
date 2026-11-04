#include "../src/common/daytona_mono_quad_anchor.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace m2vr::monoquad;
namespace {
bool Near(float x,float y,float eps=0.001f){return std::fabs(x-y)<eps;}
}
int main(){
    const View left{{-.032f,1.50f,.75f},{0.f,0.f,0.f,1.f}};
    const View right{{+.032f,1.50f,.75f},{0.f,0.f,0.f,1.f}};
    Placement placement{};
    assert(CapturePlacement(left,right,placement));
    assert(Near(placement.position.x,0.f));
    assert(Near(placement.position.y,1.50f));
    assert(Near(placement.position.z,-.90f)); // .75 - 1.65
    assert(Near(placement.width,2.f) && Near(placement.height,2.f));
    WorldFixedAnchor anchor;
    assert(anchor.CaptureOnce(left,right) && anchor.Valid());
    const auto first=anchor.Get();
    const View moved{{3.f,4.f,5.f},{0.f,0.f,0.f,1.f}};
    assert(anchor.CaptureOnce(moved,moved));
    assert(Near(anchor.Get().position.z,first.position.z));
    anchor.Reset();
    assert(!anchor.Valid());
    assert(anchor.CaptureOnce(moved,moved));
    assert(Near(anchor.Get().position.z,3.35f));
    // Yaw 90 degrees: OpenXR -Z forward should point towards -X.
    constexpr float half=.70710678118f;
    View yleft=left,yright=right;
    yleft.orientation={0.f,half,0.f,half};
    yright.orientation={0.f,-half,0.f,-half}; // antipodal quaternion
    assert(CapturePlacement(yleft,yright,placement));
    assert(Near(placement.position.x,-1.65f));
    assert(Near(placement.position.y,1.50f));
    assert(Near(placement.position.z,.75f));
    // Nonfinite, inconsistent, wildly separated eye poses reject without
    // updating a previously valid placement.
    const auto old=placement;
    yleft.position.x=std::numeric_limits<float>::quiet_NaN();
    assert(!CapturePlacement(yleft,yright,placement));
    assert(Near(placement.position.x,old.position.x));
    yleft=left;yright=right;
    yright.position.x=5.f;
    assert(!CapturePlacement(yleft,yright,placement));
    yright=right;yright.orientation={1.f,0.f,0.f,0.f};
    assert(!CapturePlacement(yleft,yright,placement));
    return 0;
}
