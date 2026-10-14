#include "../src/vr/model2_render_policy.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"Model2 eye-pass policy FAILED line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace m2vr::vr;
int main(){
    DrawEvidence d{};
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.verified_cpu_pass=RenderPass::World3D;
    d.current_render_target_is_backbuffer=true;
    d.depth_test_enabled=true;
    d.render_target_known=true;
    d.frame_pose_valid=true;
    d.scene_is_gameplay=true;
    // Calibrated scale/projection required; never invent unit scale.
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.pixel_projection_calibrated=true;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StereoGeometry);
    d.mrt_active=true;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.mrt_active=false;d.occlusion_query_active=true;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.occlusion_query_active=false;d.inside_stereo_replay=true;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.inside_stereo_replay=false;d.state_block_recording=true;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.state_block_recording=false;d.depth_test_enabled=false;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.verified_cpu_pass=RenderPass::Sky;
    REQUIRE(ChooseEyeAction(d)==EyeAction::RotationOnly);
    d.verified_cpu_pass=RenderPass::Hud2D;d.depth_test_enabled=true;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.verified_cpu_pass=RenderPass::Unknown;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    d.verified_cpu_pass=RenderPass::World3D;
    d.current_render_target_is_backbuffer=false;
    REQUIRE(ChooseEyeAction(d)==EyeAction::StockMono);
    return 0;
}
