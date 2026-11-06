#include "../src/vr/model2_head_recenter.hpp"
#include "../src/vr/model2_frame_latch.hpp"
#include "../src/vr/model2_camera_source_contract.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"recenter failure line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr;
using namespace m2vr::vr;
static bool Near(float a,float b,float epsilon=0.0005f){
    return std::fabs(a-b)<epsilon;
}
static pose::Frame Eyes(float yaw,float head_x,float head_y,float head_z,
                        std::uint64_t sequence){
    pose::Frame p{};
    p.sequence=sequence;
    p.view_flags=pose::kOrientationValid|pose::kPositionValid;
    const float s=std::sin(yaw*0.5f),c=std::cos(yaw*0.5f);
    for(int eye=0;eye<2;++eye){
        auto& e=p.eyes[eye];
        const float local=eye?0.032f:-0.032f;
        e.position_m[0]=head_x+local*std::cos(yaw);
        e.position_m[1]=head_y;
        e.position_m[2]=head_z-local*std::sin(yaw);
        e.orientation_xyzw[1]=s;
        e.orientation_xyzw[3]=c;
        e.fov_left_right_up_down[0]=-0.785398163f;
        e.fov_left_right_up_down[1]=0.785398163f;
        e.fov_left_right_up_down[2]=0.785398163f;
        e.fov_left_right_up_down[3]=-0.785398163f;
    }
    return p;
}
int main(){
    constexpr float pi=3.14159265358979323846f;
    Model2HeadRecenter rebase;
    pose::Frame output{};
    CHECK(!rebase.Anchored());
    auto initial=Eyes(pi*0.5f,5.f,1.f,10.f,10u);
    CHECK(rebase.Rebase(initial,output));
    CHECK(rebase.Anchored()&&output.sequence==10u);
    CHECK(Near(output.eyes[0].position_m[0],-0.032f));
    CHECK(Near(output.eyes[1].position_m[0],0.032f));
    CHECK(Near(output.eyes[0].position_m[1],0.f));
    CHECK(Near(output.eyes[0].position_m[2],0.f));
    CHECK(Near(output.eyes[0].orientation_xyzw[3],1.f));
    CHECK(Near(output.eyes[1].orientation_xyzw[1],0.f));
    CHECK(output.eyes[0].fov_left_right_up_down[0]==
          initial.eyes[0].fov_left_right_up_down[0]);
    // One meter of head movement: baseline yaw=90 means world -Z=local +X.
    auto moved=Eyes(pi*0.5f,5.f,1.f,9.f,11u);
    CHECK(rebase.Rebase(moved,output));
    CHECK(Near(output.eyes[0].position_m[0],1.f-0.032f));
    CHECK(Near(output.eyes[1].position_m[0],1.f+0.032f));
    // A 45 degree local turn survives a 90 degree initial LOCAL orientation.
    auto turned=Eyes(pi*0.75f,5.f,1.f,9.f,12u);
    CHECK(rebase.Rebase(turned,output));
    CHECK(Near(output.eyes[0].orientation_xyzw[1],std::sin(pi*0.125f)));
    CHECK(Near(output.eyes[0].orientation_xyzw[3],std::cos(pi*0.125f)));
    Model2FramePoseLatch latch;
    CHECK(latch.Begin(50,output));
    pose::Frame latched{};
    CHECK(latch.Read(50,latched) && latched.sequence==12u);
    CHECK(!Near(latched.eyes[0].position_m[0],latched.eyes[1].position_m[0]));
    // A corrupted fresh input does not mutate output or calibration origin.
    const pose::Frame old=output;
    auto bad=turned;bad.sequence=13u;
    bad.eyes[0].position_m[0]=std::numeric_limits<float>::infinity();
    CHECK(!rebase.Rebase(bad,output));
    CHECK(output.sequence==old.sequence);
    CHECK(Near(output.eyes[0].position_m[0],old.eyes[0].position_m[0]));
    bad=turned;bad.sequence=13u;
    bad.eyes[0].orientation_xyzw[3]=0.f;
    CHECK(!rebase.Rebase(bad,output));
    bad=turned;bad.sequence=13u;
    // Guard direct native CPU recenter callers as well as the Win64 host
    // publisher and original x86 Draw/Present source gate. A physical
    // stereo rig cannot have opposite eye orientations despite good IPD.
    bad.eyes[1].orientation_xyzw[1]=0.f;
    bad.eyes[1].orientation_xyzw[3]=1.f;
    CHECK(!rebase.Rebase(bad,output));
    CHECK(output.sequence==old.sequence);
    bad=turned;bad.sequence=13u;
    bad.eyes[1].fov_left_right_up_down[0]=-0.2f;
    bad.eyes[1].fov_left_right_up_down[1]=0.2f;
    CHECK(!rebase.Rebase(bad,output));
    CHECK(output.sequence==old.sequence);
    bad=turned;bad.sequence=13u;
    bad.eyes[1].position_m[0]=bad.eyes[0].position_m[0];
    bad.eyes[1].position_m[2]=bad.eyes[0].position_m[2];
    CHECK(!rebase.Rebase(bad,output)); // identical eyes are not stereo
    CHECK(rebase.Rebase(moved,output));
    CHECK(Near(output.eyes[0].position_m[0],1.f-0.032f));
    rebase.Reset();
    CHECK(!rebase.Anchored());
    CHECK(rebase.Rebase(moved,output));
    CHECK(Near(output.eyes[0].position_m[0],-0.032f));
    CHECK(Near(output.eyes[0].orientation_xyzw[3],1.f));
    // The real Win64 XR host now tags LOCAL-space generations in the
    // formerly reserved x86/x64 pose IPC field. A 6DoF re-center must
    // invalidate the anchored old LOCAL origin without changing any game
    // camera calibration, renderer or original D3D9 draw.
    auto local_one=Eyes(pi*0.5f,5.f,1.f,10.f,20u);
    local_one.reserved=1u;
    CHECK(rebase.Rebase(local_one,output));
    CHECK(rebase.ReferenceSpaceEpoch()==1u);
    auto local_move=Eyes(pi*0.5f,5.f,1.f,9.f,21u);
    local_move.reserved=1u;
    CHECK(rebase.Rebase(local_move,output));
    CHECK(Near(output.eyes[0].position_m[0],1.f-0.032f));
    // Headset LOCAL moved. New source positions can numerically resemble
    // older coordinates but must be treated as a new physical origin.
    auto recentered=Eyes(0.f,0.f,1.f,0.f,22u);
    recentered.reserved=2u;
    CHECK(rebase.Rebase(recentered,output));
    CHECK(rebase.ReferenceSpaceEpoch()==2u);
    CHECK(Near(output.eyes[0].position_m[0],-0.032f));
    CHECK(Near(output.eyes[1].position_m[0],0.032f));
    const auto kept=output;
    CHECK(!rebase.Rebase(local_move,output));
    CHECK(!rebase.Rebase(Eyes(0.f,0.f,1.f,0.f,23u),output));
    CHECK(rebase.ReferenceSpaceEpoch()==2u);
    CHECK(output.sequence==kept.sequence);
    auto invalid_epoch=Eyes(0.f,0.f,1.f,0.f,24u);
    invalid_epoch.reserved=3u;
    invalid_epoch.eyes[1].orientation_xyzw[3]=0.f;
    CHECK(!rebase.Rebase(invalid_epoch,output));
    CHECK(rebase.ReferenceSpaceEpoch()==2u);
    auto moved_in_new_origin=Eyes(0.f,0.5f,1.f,0.f,25u);
    moved_in_new_origin.reserved=2u;
    CHECK(rebase.Rebase(moved_in_new_origin,output));
    CHECK(Near(output.eyes[0].position_m[0],0.5f-0.032f));
    // Source camera-axis and world scale are immutable for an emulated
    // race camera epoch, not automatically inferred from the headset.
    Model2CameraEpochContract camera_epoch;
    Model2CameraCalibration cal{};
    cal.view_to_xr={1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,-1.f};
    cal.units_per_meter=100.f;
    cal.min_depth_m=0.05f;
    cal.runtime_confirmed=true;
    CHECK(camera_epoch.Admit(cal));
    CHECK(camera_epoch.Anchored());
    auto changed=cal;
    changed.units_per_meter=50.f;
    CHECK(!camera_epoch.Admit(changed));
    changed=cal;changed.min_depth_m=0.01f;
    CHECK(!camera_epoch.Admit(changed));
    changed=cal;
    changed.view_to_xr={-1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,-1.f};
    CHECK(!camera_epoch.Admit(changed));
    changed=cal;changed.runtime_confirmed=false;
    CHECK(!camera_epoch.Admit(changed));
    changed=cal;changed.units_per_meter=100.0005f;
    CHECK(camera_epoch.Admit(changed)); // tolerates numerical roundoff
    CHECK(camera_epoch.Admit(cal)); // a rejection must not mutate baseline
    camera_epoch.Reset();
    CHECK(!camera_epoch.Anchored());
    CHECK(camera_epoch.Admit(changed)); // explicit new verified epoch

    // Real binocular views may be mildly canted. Their median head
    // orientation, not the *left* eye's optical axis, defines the world
    // recenter reference. The two separate eye orientations must survive.
    constexpr float cant=8.f*pi/180.f;
    auto canted=Eyes(pi*0.5f,5.f,1.f,10.f,40u);
    for(int eye=0;eye<2;++eye){
        const float half=(pi*0.5f+(eye?-cant:cant))*0.5f;
        canted.eyes[eye].orientation_xyzw[1]=std::sin(half);
        canted.eyes[eye].orientation_xyzw[3]=std::cos(half);
    }
    Model2HeadRecenter canted_origin;
    pose::Frame canted_output{};
    CHECK(pose::CheckStereoRigGeometry(canted).valid());
    CHECK(canted_origin.Rebase(canted,canted_output));
    CHECK(canted_origin.Anchored());
    CHECK(Near(canted_output.eyes[0].orientation_xyzw[1],std::sin(cant*0.5f)));
    CHECK(Near(canted_output.eyes[1].orientation_xyzw[1],-std::sin(cant*0.5f)));
    CHECK(Near(canted_output.eyes[0].orientation_xyzw[3],std::cos(cant*0.5f)));
    CHECK(Near(canted_output.eyes[1].orientation_xyzw[3],std::cos(cant*0.5f)));
    CHECK(Near(canted_output.eyes[0].position_m[0],-0.032f));
    CHECK(Near(canted_output.eyes[1].position_m[0],0.032f));
    auto canted_moved=canted;
    canted_moved.sequence=41u;
    for(auto& eye:canted_moved.eyes)eye.position_m[2]-=1.f;
    CHECK(canted_origin.Rebase(canted_moved,canted_output));
    CHECK(Near(canted_output.eyes[0].position_m[0],1.f-0.032f));
    CHECK(Near(canted_output.eyes[1].position_m[0],1.f+0.032f));
    // OpenXR is free to flip the sign of either equivalent eye
    // quaternion. Re-anchoring a new LOCAL epoch must not rotate the
    // driver by ~180 degrees or prefer whichever eye has positive w.
    auto sign_flipped=canted;
    sign_flipped.sequence=42u;
    sign_flipped.reserved=5u;
    for(float& q:sign_flipped.eyes[1].orientation_xyzw)q=-q;
    CHECK(pose::CheckStereoRigGeometry(sign_flipped).valid());
    CHECK(canted_origin.Rebase(sign_flipped,canted_output));
    CHECK(canted_origin.ReferenceSpaceEpoch()==5u);
    CHECK(Near(canted_output.eyes[0].orientation_xyzw[1],std::sin(cant*0.5f)));
    CHECK(Near(canted_output.eyes[1].orientation_xyzw[1],-std::sin(cant*0.5f)));
    CHECK(Near(canted_output.eyes[0].position_m[0],-0.032f));
    CHECK(Near(canted_output.eyes[1].position_m[0],0.032f));
    return 0;
}
