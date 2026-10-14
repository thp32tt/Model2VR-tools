#include "../src/vr/model2_cpu_stereo.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"Model2 calibrated eye projection FAILED %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace m2vr;
using namespace m2vr::vr;
int main(){
    Model2CameraCalibration cal;
    // Deliberately uncalibrated on startup: no guessing at world scale.
    DrawEvidence pass{RenderPass::World3D,true,true,true,false,false,false,false,true,true,true};
    pose::Frame xr{};
    xr.sequence=1;
    xr.view_flags=pose::kOrientationValid|pose::kPositionValid;
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
    const auto near=ProjectModel2Stereo(front,xr,cal,pass);
    REQUIRE(near.stereo&&near.eyes[0].valid&&near.eyes[1].valid);
    REQUIRE(near.eyes[0].ndc_x>near.eyes[1].ndc_x);
    REQUIRE(std::abs(near.eyes[0].ndc_x-0.016f)<0.001f);
    REQUIRE(std::abs(near.eyes[1].ndc_x+0.016f)<0.001f);
    const auto far=ProjectModel2Stereo(CpuPoint{0,0,100},xr,cal,pass);
    REQUIRE(far.stereo);
    REQUIRE(near.eyes[0].ndc_x-near.eyes[1].ndc_x >
            far.eyes[0].ndc_x-far.eyes[1].ndc_x);
    for(auto kind:{RenderPass::Hud2D,RenderPass::Unknown,RenderPass::Sky,RenderPass::Auxiliary}){
        pass.verified_cpu_pass=kind;
        REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    }
    pass.verified_cpu_pass=RenderPass::World3D;
    pass.depth_test_enabled=false;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    pass.depth_test_enabled=true;
    xr.view_flags=pose::kOrientationValid;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    xr.view_flags=pose::kOrientationValid|pose::kPositionValid;
    cal.view_to_xr={1,0,0,0,1,0,0,0,0};
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
    xr.eyes[0].orientation_xyzw[3]=0.f;
    REQUIRE(!ProjectModel2Stereo(front,xr,cal,pass).stereo);
    return 0;
}
