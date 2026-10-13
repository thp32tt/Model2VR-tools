#include "../src/vr/stereo_core.h"
#include <cassert>
#include <cmath>

using namespace m2vr::vr;
bool almost(float a,float b){return std::fabs(a-b)<1e-4f;}
int main(){
    GameProfile profile;
    assert(profile.set_caller(0x1122,PassClass::World3D));
    assert(profile.classify(0x1122)==PassClass::World3D);
    assert(profile.classify(0x9999)==PassClass::Unknown);
    ViewConfig config;
    config.units_per_meter=100.0f;
    config.ipd_m=0.063f;
    assert(profile.set_view_config(config));
    StereoCoordinator stereo;
    HeadPose identity;
    assert(stereo.begin_frame(1,identity,profile));
    assert(!stereo.begin_frame(1,identity,profile));
    auto out=stereo.transform({0,0,100},PassClass::World3D);
    assert(out.stereo);
    assert(almost(out.left.x,3.15f));
    assert(almost(out.right.x,-3.15f));
    assert(almost(out.left.z,100));
    auto both=stereo.project_preview({0,0,100},PassClass::World3D);
    assert(both[0].valid && both[1].valid);
    assert(almost(both[0].x_over_depth,-both[1].x_over_depth));
    auto hud=stereo.transform({0,0,100},PassClass::Hud2D);
    assert(!hud.stereo && almost(hud.left.x,hud.right.x));
    auto unknown=stereo.transform({1,2,100},PassClass::Unknown);
    assert(!unknown.stereo);
    HeadPose moved; moved.valid=true; moved.position_m={0.5f,0,0};
    config.allow_head_position=true;
    assert(profile.set_view_config(config));
    assert(stereo.begin_frame(2,moved,profile));
    out=stereo.transform({0,0,100},PassClass::World3D);
    assert(almost(out.left.x,-46.85f));
    config.units_per_meter=-1;
    assert(!profile.set_view_config(config));
    return 0;
}
