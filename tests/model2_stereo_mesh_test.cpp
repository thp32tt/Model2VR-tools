#include "../src/vr/model2_stereo_mesh.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"failed line %d\n",__LINE__); return 1; } } while(0)
using namespace m2vr;
using namespace m2vr::vr;
int main() {
 DrawEvidence pass{RenderPass::World3D,true,true,true,false,false,false,false,true,true,true};
 Model2CameraCalibration cal{};
 cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
 cal.units_per_meter=1.f;
 cal.runtime_confirmed=true;
 pose::Frame frame{};
 frame.sequence=1;
 frame.view_flags=pose::kOrientationValid|pose::kPositionValid;
 for (int eye=0;eye<2;++eye) {
  frame.eyes[eye].orientation_xyzw[3]=1.f;
  frame.eyes[eye].position_m[0]=eye ? 0.032f : -0.032f;
  auto& f=frame.eyes[eye].fov_left_right_up_down;
  f[0]=-0.785398163f; f[1]=0.785398163f;
  f[2]=0.785398163f; f[3]=-0.785398163f;
 }
 const std::array<CpuPoint,4> points{{{-0.2f,-0.2f,2.f},{0.2f,-0.2f,2.f},{0.f,0.2f,2.f},{0.2f,0.2f,2.f}}};
 const std::uint32_t indices[]={0,1,2,1,3,2};
 std::vector<StereoTriangle> result;
 CHECK(BuildStereoTriangles(points.data(),points.size(),indices,6,frame,cal,pass,result));
 CHECK(result.size()==2);
 CHECK(result[0].eyes[0][0].ndc_x>result[0].eyes[1][0].ndc_x);
 CHECK(std::abs(result[0].eyes[0][0].depth_m-2.f)<0.0001f);
 const float preserved=result[0].eyes[0][0].ndc_x;
 const std::uint32_t invalid[]={0,1,2,1,99,2};
 CHECK(!BuildStereoTriangles(points.data(),points.size(),invalid,6,frame,cal,pass,result));
 CHECK(result.size()==2 && result[0].eyes[0][0].ndc_x==preserved);
 pass.verified_cpu_pass=RenderPass::Hud2D;
 CHECK(!BuildStereoTriangles(points.data(),points.size(),indices,6,frame,cal,pass,result));
 pass.verified_cpu_pass=RenderPass::World3D;
 cal.runtime_confirmed=false;
 CHECK(!BuildStereoTriangles(points.data(),points.size(),indices,6,frame,cal,pass,result));
 cal.runtime_confirmed=true;
 const CpuPoint behind[]={{0,0,2},{0,0,-1},{0.2f,0,2}};
 CHECK(!BuildStereoTriangles(behind,3,indices,3,frame,cal,pass,result));
 const CpuPoint outside[]={{0,0,2},{100,0,2},{0.2f,0,2}};
 CHECK(!BuildStereoTriangles(outside,3,indices,3,frame,cal,pass,result));
 CHECK(!BuildStereoTriangles(points.data(),points.size(),indices,5,frame,cal,pass,result));
 CHECK(result.size()==2);
 return 0;
}
