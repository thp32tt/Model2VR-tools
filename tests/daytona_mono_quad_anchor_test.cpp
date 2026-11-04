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
    assert(Near(Dot(placement.orientation,placement.orientation),1.f));
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
    // Downward initial gaze should NOT tilt/move the 2D theater below
    // eye height or give it 3D scene camera pitch.
    constexpr float s=0.382683432365f,c=0.923879532511f;
    View downleft=left,downright=right;
    downleft.orientation={s,0.f,0.f,c};
    downright.orientation={s,0.f,0.f,c};
    assert(CapturePlacement(downleft,downright,placement));
    assert(Near(placement.position.y,1.50f));
    assert(Near(placement.position.z,-.90f));
    assert(Near(placement.orientation.x,0.f));
    assert(Near(placement.orientation.z,0.f));
    assert(CapturePlacement(left,right,placement,2.25f,2.5f));
    assert(Near(placement.position.z,-1.50f));
    assert(Near(placement.width,2.5f) && Near(placement.height,2.5f));
    assert(!CapturePlacement(left,right,placement,0.3f,2.f));
    assert(!CapturePlacement(left,right,placement,1.65f,8.f));
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
