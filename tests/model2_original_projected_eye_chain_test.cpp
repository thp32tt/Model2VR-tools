// Authenticated ORIGINAL x86 D3D9 projected IB fan -> real CPU projector
// join -> 64-triangle independent OpenXR-view L/R CPU raster contract.
// Synthetic inputs exercise the real native bridge primitives; they never
// authenticate the Daytona game world, camera calibration or live headset.
#include "../src/probe/d3d9_vb_index_window.hpp"
#include "../src/probe/project_draw_correlator.hpp"
#include "../src/probe/model2_camera_projection_fit.hpp"
#include "../src/vr/model2_stereo_raster.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"original projected-eye chain line %d: %s\n",__LINE__,#x);return 1;}}while(0)

int main() {
    using namespace m2vr;
    using namespace m2vr::drawprobe;
    using namespace m2vr::vr;
    constexpr std::uint32_t kTriangles=64u;
    constexpr std::uint32_t kPoints=kTriangles+2u;
    constexpr std::uint32_t kCorners=kTriangles*3u;
    constexpr std::uint32_t kFvf=D3DFVF_XYZRHW|D3DFVF_TEX1;
    constexpr float kPi=3.14159265358979323846f;
    std::vector<std::array<float,6>> original(kPoints);
    std::vector<std::array<float,3>> original_xyz(kPoints);
    std::vector<std::uint16_t> original_indices(kPoints);
    ProjectDrawCorrelator projector{};
    for(std::uint32_t i=0u;i<kPoints;++i) {
        const float angle=(2.f*kPi*float(i==0u?0u:i-1u))/
            float(kPoints-1u);
        const float x=i?0.8f*std::cos(angle):0.f;
        const float y=i?0.8f*std::sin(angle):0.f;
        constexpr float z=5.f;
        const float screen_x=320.f+400.f*x/z;
        const float screen_y=240.f-380.f*y/z;
        original[i]={screen_x,screen_y,0.5f,0.25f,
                     float(i%11u)*0.04f,float(i%7u)*0.07f};
        original_xyz[i]={x,y,z};
        original_indices[i]=static_cast<std::uint16_t>(500u+i);
        const float after[3]{screen_x,screen_y,0.5f};
        projector.Capture(73u,0x004bb400u,original_xyz[i].data(),after,
                          0x1000u,0x12345678u,0x44u);
    }
    VBVertexWindow window{};
    CHECK(RemapIndexedTopology(
        reinterpret_cast<const std::uint8_t*>(original_indices.data()),
        original_indices.size()*sizeof(std::uint16_t),
        D3DFMT_INDEX16,D3DPT_TRIANGLEFAN,
        -500,500u,kPoints,kTriangles,window));
    CHECK(window.first==0u && window.count==kPoints &&
          window.indices.size()==kCorners);
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(original.data());
    const std::size_t byte_count=original.size()*sizeof(original[0]);
    std::vector<std::uint8_t> expanded{};
    CHECK(!ExpandProjectedVBWindow(
        bytes,byte_count,kFvf,24u,window,expanded)); // passive 8-triangle limit
    CHECK(ExpandProjectedVBWindow(bytes,byte_count,kFvf,24u,window,expanded,
                                 ProjectedExpansionUse::AuthenticatedWorld));
    CHECK(expanded.size()==std::size_t(kCorners)*24u);
    std::array<ProjectedDrawVertex,kCorners> gpu_corners{};
    for(std::size_t i=0u;i<gpu_corners.size();++i) {
        ProjectedUPWorldVertex decoded{};
        CHECK(DecodeProjectedUPWorldVertex(
            expanded.data()+i*24u,kFvf,24u,decoded));
        gpu_corners[i]={decoded.screen_xy[0],decoded.screen_xy[1]};
    }
    const auto joined=projector.MatchTriangleList(
        73u,gpu_corners.data(),gpu_corners.size(),window.indices.data());
    CHECK(joined.StableOriginalProjectionProvenance() &&
          joined.inspected==64u && joined.complete==64u);
    CHECK(joined.distinct_captures==66u &&
          joined.fit_sample_count==66u &&
          joined.camera_sample_count==192u &&
          joined.legitimate_shared_vertices==126u);
    const auto optics=FitSourcePerspective(joined.fit_samples.data(),
        joined.fit_sample_count,0u,0u,640u,480u);
    CHECK(optics.plausible && optics.accepted_samples==66u);
    CHECK(std::abs(optics.center_x-320.f)<0.1f &&
          std::abs(optics.center_y-240.f)<0.1f);
    CHECK(std::abs(optics.scale_x-400.f)<1.f &&
          std::abs(optics.scale_y+380.f)<1.f);

    // Source identity and original GPU winding survive expansion, while
    // the two HMD eyes receive geometry in independent tracked poses.
    std::vector<CpuPoint> world_corners(kCorners);
    std::vector<std::uint32_t> eye_indices(kCorners);
    for(std::size_t i=0;i<world_corners.size();++i) {
        const auto& src=joined.camera_samples[i].source_xyz;
        world_corners[i]={src[0],src[1],src[2]};
        eye_indices[i]=static_cast<std::uint32_t>(i);
        const auto& expected=original_xyz[window.indices[i]];
        CHECK(world_corners[i].x==expected[0] &&
              world_corners[i].y==expected[1] &&
              world_corners[i].z==expected[2]);
    }
    DrawEvidence pass{
        RenderPass::World3D,true,true,true,false,false,false,false,true,true,true};
    Model2CameraCalibration calibration{};
    calibration.view_to_xr={1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,-1.f};
    calibration.units_per_meter=1.f;
    calibration.min_depth_m=0.05f;
    calibration.runtime_confirmed=true; // SYNTHETIC TEST ONLY, not game proof
    pose::Frame predicted{};
    predicted.sequence=102u;
    predicted.view_flags=pose::kOrientationValid|pose::kPositionValid;
    for(std::size_t eye=0;eye<2u;++eye) {
        predicted.eyes[eye].orientation_xyzw[3]=1.f;
        predicted.eyes[eye].position_m[0]=eye?0.032f:-0.032f;
        predicted.eyes[eye].fov_left_right_up_down={
            -0.785398163f,0.785398163f,0.785398163f,-0.785398163f};
    }
    Model2FramePoseLatch latch{};
    CHECK(latch.Begin(811u,predicted));
    ClippedStereoBatch clipped{};
    CHECK(BuildClippedStereoBatch(
        world_corners.data(),world_corners.size(),
        eye_indices.data(),eye_indices.size(),
        811u,latch,calibration,pass,clipped));
    CHECK(clipped.game_frame==811u && clipped.pose_sequence==102u);
    CHECK(clipped.eyes[0].size()==kTriangles &&
          clipped.eyes[1].size()==kTriangles);
    CHECK(clipped.eyes[0][0].vertices[0].ndc_x>
          clipped.eyes[1][0].vertices[0].ndc_x);
    StereoWorldRaster raster{};
    std::array<std::uint32_t,kTriangles> colors{};
    colors.fill(0xff00cc55u);
    CHECK(raster.Draw(clipped,colors.data(),colors.size()));
    CHECK(raster.WrittenEyePixels(0u)>0u &&
          raster.WrittenEyePixels(1u)>0u &&
          raster.StereoPairUsable());
    CHECK(raster.Eye(0u).size()==transport::kBytesPerEye &&
          raster.Eye(1u).size()==transport::kBytesPerEye);
    // Reusing an earlier physical Draw's CPU pixels must never become
    // a second authentic source in this Present even if the XY is equal.
    projector.RetireOriginalDraw(73u);
    CHECK(!projector.MatchTriangleList(
        73u,gpu_corners.data(),gpu_corners.size(),
        window.indices.data()).StableOriginalProjectionProvenance());
    // No headset calibration or scene owner means no independent stereo.
    calibration.runtime_confirmed=false;
    CHECK(!BuildClippedStereoBatch(
        world_corners.data(),world_corners.size(),
        eye_indices.data(),eye_indices.size(),
        811u,latch,calibration,pass,clipped));
    pass.verified_cpu_pass=RenderPass::Hud2D;
    CHECK(!BuildClippedStereoBatch(
        world_corners.data(),world_corners.size(),
        eye_indices.data(),eye_indices.size(),
        811u,latch,calibration,pass,clipped));
    return 0;
}
