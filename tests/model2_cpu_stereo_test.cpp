#include "../src/vr/model2_cpu_stereo.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <utility>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"Model2 calibrated eye projection FAILED %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace m2vr;
using namespace m2vr::vr;
int main(){
    Model2CameraCalibration cal;
    // Deliberately uncalibrated on startup: no guessing at world scale.
    DrawEvidence pass{RenderPass::World3D,true,true,true,false,false,false,false,true,true,true};
    pose::Frame xr{};
    xr.sequence=1;
    xr.view_flags=pose::kFullSixDofTracked;
    for(int e=0;e<2;++e){
        xr.eyes[e].position_m[0]=e==0?-0.032f:0.032f;
        xr.eyes[e].fov_left_right_up_down[0]=-0.785398163f;
        xr.eyes[e].fov_left_right_up_down[1]=0.785398163f;
        xr.eyes[e].fov_left_right_up_down[2]=0.785398163f;
        xr.eyes[e].fov_left_right_up_down[3]=-0.785398163f;
    }
    const CpuPoint front{0,0,2};
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
    cal.units_per_meter=1.f;cal.runtime_confirmed=true;
    REQUIRE(ValidCalibration(cal));
    const auto nearPair=ProjectModel2Stereo(front,xr,cal,pass);
    REQUIRE(nearPair.stereo&&nearPair.eyes[0].valid&&nearPair.eyes[1].valid);
    REQUIRE(nearPair.eyes[0].ndc_x>nearPair.eyes[1].ndc_x);
    REQUIRE(std::abs(nearPair.eyes[0].ndc_x-0.016f)<0.001f);
    REQUIRE(std::abs(nearPair.eyes[1].ndc_x+0.016f)<0.001f);
    const auto farPair=ProjectModel2Stereo(CpuPoint{0,0,100},xr,cal,pass);
    REQUIRE(farPair.stereo);
    REQUIRE(nearPair.eyes[0].ndc_x-nearPair.eyes[1].ndc_x >
            farPair.eyes[0].ndc_x-farPair.eyes[1].ndc_x);
    for(auto kind:{RenderPass::Hud2D,RenderPass::Unknown,RenderPass::Sky,RenderPass::Auxiliary}){
        pass.verified_cpu_pass=kind;
        REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    }
    pass.verified_cpu_pass=RenderPass::World3D;
    pass.depth_test_enabled=false;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    pass.depth_test_enabled=true;
    // VALID but not TRACKED is allowed in some mono GPU fallbacks,
    // never for independently projected physical CPU World3D eyes.
    xr.view_flags=pose::kFullSixDofTracked & ~pose::kPositionTracked;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr.view_flags=pose::kFullSixDofTracked & ~pose::kOrientationTracked;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr.view_flags=pose::kOrientationValid;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr.view_flags=pose::kFullSixDofTracked;
    cal.view_to_xr={1,0,0,0,1,0,0,0,0};
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
    xr.eyes[0].orientation_xyzw[3]=0.f;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr.eyes[0].orientation_xyzw[3]=1.f;
    const auto real_pair=xr;
    // Same-eye mono or swapped OpenXR indices must NEVER be labeled as
    // two independently projected physical game-world views.
    xr.eyes[1].position_m[0]=xr.eyes[0].position_m[0];
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr=real_pair;
    std::swap(xr.eyes[0].position_m[0],xr.eyes[1].position_m[0]);
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr=real_pair;
    xr.eyes[1].fov_left_right_up_down[0]=-0.21f;
    xr.eyes[1].fov_left_right_up_down[1]=0.22f;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr=real_pair;
    xr.eyes[1].orientation_xyzw[1]=1.f;
    xr.eyes[1].orientation_xyzw[3]=0.f;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr=real_pair;
    REQUIRE(ProjectModel2Stereo(front,xr,cal,pass).stereo);
    // Mildly canted real HMD lenses remain valid independent eye poses.
    const float cant=8.f*3.14159265358979323846f/180.f;
    xr.eyes[0].orientation_xyzw[1]=std::sin(cant*0.5f);
    xr.eyes[0].orientation_xyzw[3]=std::cos(cant*0.5f);
    xr.eyes[1].orientation_xyzw[1]=-std::sin(cant*0.5f);
    xr.eyes[1].orientation_xyzw[3]=std::cos(cant*0.5f);
    REQUIRE(ProjectModel2Stereo(front,xr,cal,pass).stereo);
    return 0;
}
