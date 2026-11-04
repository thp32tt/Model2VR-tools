#include "../src/vr/model2_head_recenter.hpp"
#include "../src/vr/model2_frame_latch.hpp"
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
    return 0;
}
