#pragma once
// Conservative game render-pass policy inspired by the OutRun R33 safety
// boundary and PPSSPP/DolphinXR per-game pass overrides. No licensed code copied.
#include <cstdint>
namespace m2vr::vr {
enum class RenderPass : std::uint8_t { Unknown, World3D, Hud2D, Sky, Auxiliary };
enum class EyeAction : std::uint8_t { StockMono, StereoGeometry, RotationOnly };
struct DrawEvidence {
    RenderPass verified_cpu_pass=RenderPass::Unknown;
    bool current_render_target_is_backbuffer=false;
    bool depth_test_enabled=false;
    bool render_target_known=false;
    bool inside_stereo_replay=false;
    bool state_block_recording=false;
    bool mrt_active=false;
    bool occlusion_query_active=false;
    bool frame_pose_valid=false;
    bool scene_is_gameplay=false;
    bool pixel_projection_calibrated=false;
};
constexpr EyeAction ChooseEyeAction(const DrawEvidence& d) noexcept {
    if (!d.frame_pose_valid || !d.scene_is_gameplay ||
        !d.render_target_known || !d.current_render_target_is_backbuffer ||
        d.inside_stereo_replay || d.state_block_recording ||
        d.mrt_active || d.occlusion_query_active ||
        !d.pixel_projection_calibrated) return EyeAction::StockMono;
    if (d.verified_cpu_pass==RenderPass::Sky && !d.depth_test_enabled)
        return EyeAction::RotationOnly;
    if (d.verified_cpu_pass==RenderPass::World3D && d.depth_test_enabled)
        return EyeAction::StereoGeometry;
    return EyeAction::StockMono;
}
static_assert(ChooseEyeAction(DrawEvidence{})==EyeAction::StockMono);
static_assert(ChooseEyeAction(DrawEvidence{
    RenderPass::World3D,true,true,true,false,false,false,false,true,true,true
})==EyeAction::StereoGeometry);
static_assert(ChooseEyeAction(DrawEvidence{
    RenderPass::Hud2D,true,true,true,false,false,false,false,true,true,true
})==EyeAction::StockMono);
static_assert(ChooseEyeAction(DrawEvidence{
    RenderPass::World3D,true,false,true,false,false,false,false,true,true,true
})==EyeAction::StockMono);
static_assert(ChooseEyeAction(DrawEvidence{
    RenderPass::World3D,true,true,true,false,false,true,false,true,true,true
})==EyeAction::StockMono);
} // namespace m2vr::vr
