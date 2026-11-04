// Standalone CPU stereo RGBA producer test. No Model 2 game or FFB needed.
#include "../src/vr/model2_stereo_raster.hpp"
#include <array>
#include <cstdio>
#include <cstdint>
#include <vector>
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"stereo raster failed at %d: %s\n",__LINE__,#c);return 1;}}while(0)
using namespace m2vr;
using namespace m2vr::vr;
struct Mark {std::uint64_t count=0;double sum_x=0;};
Mark Scan(const std::vector<std::uint8_t>& pixels,std::uint32_t rgb) {
    Mark out{};
    for(std::size_t i=0;i<pixels.size()/4;++i) {
        const auto p=i*4;
        if(pixels[p]==((rgb>>16)&255u) &&
           pixels[p+1]==((rgb>>8)&255u) &&
           pixels[p+2]==(rgb&255u)) {
            ++out.count;out.sum_x+=double(i%StereoWorldRaster::Width);
        }
    }
    return out;
}
int main() {
    DrawEvidence world{RenderPass::World3D,true,true,true,
        false,false,false,false,true,true,true};
    Model2CameraCalibration cal{};
    cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
    cal.units_per_meter=1.f;
    cal.min_depth_m=0.05f;
    cal.runtime_confirmed=true;
    pose::Frame frame{};
    frame.sequence=11;frame.view_flags=pose::kOrientationValid|pose::kPositionValid;
    for(int i=0;i<2;++i) {
        frame.eyes[i].position_m[0]=i?0.04f:-0.04f;
        frame.eyes[i].orientation_xyzw[3]=1.f;
        frame.eyes[i].fov_left_right_up_down[0]=-0.785398f;
        frame.eyes[i].fov_left_right_up_down[1]=0.785398f;
        frame.eyes[i].fov_left_right_up_down[2]=0.785398f;
        frame.eyes[i].fov_left_right_up_down[3]=-0.785398f;
    }
    Model2FramePoseLatch latch;
    CHECK(latch.Begin(100,frame));
    const std::uint32_t tri[]={0,1,2};
    const std::uint32_t red[]={0xDA3412u},blue[]={0x2B4AF0u};
    const std::array<CpuPoint,3> near_points{{
        {-0.4f,-0.4f,2.f},{0.4f,-0.4f,2.f},{0.f,0.4f,2.f}}};
    const std::array<CpuPoint,3> far_points{{
        {-0.8f,-0.8f,4.f},{0.8f,-0.8f,4.f},{0.f,0.8f,4.f}}};
    ClippedStereoBatch near_batch{},far_batch{};
    CHECK(BuildClippedStereoBatch(near_points.data(),3,tri,3,
                                  100,latch,cal,world,near_batch));
    CHECK(BuildClippedStereoBatch(far_points.data(),3,tri,3,
                                  100,latch,cal,world,far_batch));
    CHECK(near_batch.eyes[0][0].source_triangle==0);
    CHECK(near_batch.eyes[1][0].source_triangle==0);
    StereoWorldRaster surface;
    CHECK(!surface.StereoPairUsable());
    CHECK(surface.WrittenEyePixels(0)==0u);
    CHECK(surface.Draw(near_batch,red,1));
    CHECK(surface.StereoPairUsable());
    CHECK(surface.WrittenEyePixels(0)>0u);
    CHECK(surface.WrittenEyePixels(1)>0u);
    const Mark left=Scan(surface.Eye(0),red[0]);
    const Mark right=Scan(surface.Eye(1),red[0]);
    CHECK(left.count>1000 && right.count>1000);
    // Real stereo parallax (not a duplicated monocular screenshot).
    CHECK(left.sum_x/left.count > right.sum_x/right.count+8.0);
    // Far polygon drawn after near does NOT overwrite nearer pixels.
    CHECK(surface.Draw(far_batch,blue,1));
    CHECK(Scan(surface.Eye(0),red[0]).count==left.count);
    CHECK(Scan(surface.Eye(1),red[0]).count==right.count);
    // Parallax leaves narrow far-edge fringes: do not require zero blue.
    CHECK(Scan(surface.Eye(0),blue[0]).count < left.count/2);
    CHECK(surface.AcceptedTriangles()>=4);
    CHECK(surface.DrawnPixels()>2000);
    // Original game geometry may clip an entire eye: never publish a
    // black eye or two identical eye buffers as native binocular VR.
    surface.Clear();
    auto one_eye=near_batch;
    one_eye.eyes[1].clear();
    CHECK(surface.Draw(one_eye,red,1));
    CHECK(surface.WrittenEyePixels(0)>0u);
    CHECK(surface.WrittenEyePixels(1)==0u);
    CHECK(!surface.StereoPairUsable());
    surface.Clear();
    auto identical=near_batch;
    identical.eyes[1]=identical.eyes[0];
    CHECK(surface.Draw(identical,red,1));
    CHECK(surface.WrittenEyePixels(1)>0u);
    CHECK(!surface.StereoPairUsable());
    // Inverse order also obeys depth.
    surface.Clear();
    CHECK(surface.Draw(far_batch,blue,1));
    CHECK(surface.Draw(near_batch,red,1));
    CHECK(Scan(surface.Eye(0),red[0]).count>1000);
    // Near-plane clipping yields new independent triangle fans per eye.
    const std::array<CpuPoint,3> clipped{{
        {-0.2f,-0.2f,2.f},{0.2f,-0.2f,2.f},{0.f,0.2f,0.01f}}};
    ClippedStereoBatch clipped_batch{};
    CHECK(BuildClippedStereoBatch(clipped.data(),3,tri,3,
                                  100,latch,cal,world,clipped_batch));
    CHECK(surface.Draw(clipped_batch,red,1));
    // Failure must not mutate last accepted eye pixel state.
    const auto first_before=surface.Eye(0)[4*512*StereoWorldRaster::Width+4*512];
    const auto count_before=surface.AcceptedTriangles();
    auto corrupt=near_batch;
    corrupt.eyes[1][0].source_triangle=99;
    CHECK(!surface.Draw(corrupt,red,1));
    CHECK(surface.AcceptedTriangles()==count_before);
    CHECK(surface.Eye(0)[4*512*StereoWorldRaster::Width+4*512]==first_before);
    CHECK(!surface.Draw(near_batch,nullptr,1));
    world.verified_cpu_pass=RenderPass::Hud2D;
    ClippedStereoBatch rejected{};
    CHECK(!BuildClippedStereoBatch(near_points.data(),3,tri,3,
                                  100,latch,cal,world,rejected));
    return 0;
}
