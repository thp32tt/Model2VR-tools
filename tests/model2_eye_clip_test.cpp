#include "../src/vr/model2_eye_clip.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"eye clip failure line %d: %s\n",__LINE__,#x);return 1;} } while(0)
using namespace m2vr;
using namespace m2vr::vr;
static bool Inside(const ClippedStereoBatch& batch, float min_depth) {
 for (const auto& eye:batch.eyes)
  for (const auto& triangle:eye)
   for (const auto& v:triangle.vertices)
    if (!v.valid || !std::isfinite(v.ndc_x) || !std::isfinite(v.ndc_y) ||
        !std::isfinite(v.depth_m) || v.depth_m<min_depth-1.e-5f ||
        v.ndc_x < -1.f || v.ndc_x > 1.f || v.ndc_y < -1.f || v.ndc_y > 1.f)
     return false;
 return true;
}
int main() {
 DrawEvidence world{RenderPass::World3D,true,true,true,false,false,false,false,true,true,true};
 Model2CameraCalibration cal{};
 cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
 cal.units_per_meter=1.f;cal.min_depth_m=0.05f;cal.runtime_confirmed=true;
 pose::Frame frame{};
 frame.sequence=9;
 frame.view_flags=pose::kOrientationValid|pose::kPositionValid;
 for (int i=0;i<2;++i) {
  auto& eye=frame.eyes[i];
  eye.position_m[0]=i?0.032f:-0.032f;
  eye.orientation_xyzw[3]=1.f;
  auto& f=eye.fov_left_right_up_down;
  f[0]=-0.785398163f;f[1]=0.785398163f;
  f[2]=0.785398163f;f[3]=-0.785398163f;
 }
 Model2FramePoseLatch latch;
 CHECK(latch.Begin(100,frame));
 ClippedStereoBatch output{};
 const std::array<CpuPoint,3> inside{{{-0.2f,-0.2f,2.f},{0.2f,-0.2f,2.f},{0.f,0.2f,2.f}}};
 const std::uint32_t tri[]={0,1,2};
 CHECK(BuildClippedStereoBatch(inside.data(),inside.size(),tri,3,100,latch,cal,world,output));
 CHECK(output.game_frame==100 && output.pose_sequence==9);
 CHECK(output.eyes[0].size()==1 && output.eyes[1].size()==1);
 CHECK(output.eyes[0][0].vertices[0].ndc_x>output.eyes[1][0].vertices[0].ndc_x);
 CHECK(Inside(output,cal.min_depth_m));
 // Side-plane crossing must clip instead of discarding the entire draw.
 const std::array<CpuPoint,3> edge{{{0.f,-0.3f,2.f},{4.f,0.f,2.f},{0.f,0.3f,2.f}}};
 CHECK(BuildClippedStereoBatch(edge.data(),edge.size(),tri,3,100,latch,cal,world,output));
 CHECK(output.eyes[0].size()>=1 && output.eyes[1].size()>=1);
 CHECK(Inside(output,cal.min_depth_m));
 // A near-plane crossing is clipped in homogeneous space before division.
 const std::array<CpuPoint,3> near_points{{{-0.2f,-0.2f,2.f},{0.2f,-0.2f,2.f},{0.f,0.2f,0.01f}}};
 CHECK(BuildClippedStereoBatch(near_points.data(),near_points.size(),tri,3,100,latch,cal,world,output));
 CHECK(!output.eyes[0].empty() && !output.eyes[1].empty());
 CHECK(Inside(output,cal.min_depth_m));
 // A tiny triangle at the left edge can be visible to only one eye.
 const std::array<CpuPoint,3> monocular{{{-1.99f,-0.1f,2.f},{-1.97f,-0.1f,2.f},{-1.98f,0.1f,2.f}}};
 CHECK(BuildClippedStereoBatch(monocular.data(),monocular.size(),tri,3,100,latch,cal,world,output));
 CHECK(!output.eyes[0].empty() && output.eyes[1].empty());
 CHECK(Inside(output,cal.min_depth_m));
 // Fully offscreen world geometry is valid and produces no eye triangles.
 const std::array<CpuPoint,3> offscreen{{{100.f,0.f,2.f},{101.f,0.f,2.f},{100.f,1.f,2.f}}};
 CHECK(BuildClippedStereoBatch(offscreen.data(),offscreen.size(),tri,3,100,latch,cal,world,output));
 CHECK(output.eyes[0].empty() && output.eyes[1].empty());
 const auto saved_frame=output.game_frame;
 const std::uint32_t bad[]={0,1,3};
 CHECK(!BuildClippedStereoBatch(inside.data(),inside.size(),bad,3,100,latch,cal,world,output));
 CHECK(output.game_frame==saved_frame && output.eyes[0].empty());
 world.verified_cpu_pass=RenderPass::Hud2D;
 CHECK(!BuildClippedStereoBatch(inside.data(),inside.size(),tri,3,100,latch,cal,world,output));
 world.verified_cpu_pass=RenderPass::World3D;
 CHECK(!BuildClippedStereoBatch(inside.data(),inside.size(),tri,3,101,latch,cal,world,output));
 cal.runtime_confirmed=false;
 CHECK(!BuildClippedStereoBatch(inside.data(),inside.size(),tri,3,100,latch,cal,world,output));
 cal.runtime_confirmed=true;
 const std::array<CpuPoint,3> corrupt{{{0.f,0.f,2.f},{std::numeric_limits<float>::infinity(),0.f,2.f},{0.f,1.f,2.f}}};
 CHECK(!BuildClippedStereoBatch(corrupt.data(),corrupt.size(),tri,3,100,latch,cal,world,output));
 CHECK(output.game_frame==saved_frame && output.eyes[1].empty());
 latch.Reset();
 CHECK(!BuildClippedStereoBatch(inside.data(),inside.size(),tri,3,100,latch,cal,world,output));
 return 0;
}
