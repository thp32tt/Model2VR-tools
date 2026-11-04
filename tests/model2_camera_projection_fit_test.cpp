#include "../src/probe/model2_camera_projection_fit.hpp"
#include "../src/probe/verified_world_projection_epoch.hpp"
#include <array>
#include <cstdio>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"camera projection line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    std::array<SourceProjectionSample,6> source{};
    const float x[6]={-2.f,1.f,2.f,-1.f,1.5f,0.f};
    const float y[6]={-1.f,2.f,-2.f,1.5f,0.7f,2.f};
    const float z[6]={5.f,5.f,8.f,8.f,10.f,5.f};
    for(unsigned i=0;i<6;++i){
        source[i].source_xyz={x[i],y[i],z[i]};
        source[i].screen_xy={320.f+400.f*x[i]/z[i],
                             240.f-380.f*y[i]/z[i]};
    }
    const auto camera=FitSourcePerspective(source.data(),6,0,0,640,480);
    CHECK(camera.plausible && camera.accepted_samples==6);
    CHECK(camera.depth_positive && camera.rms_pixels<0.01f);
    CHECK(camera.center_x>319.f && camera.center_x<321.f);
    CHECK(camera.center_y>239.f && camera.center_y<241.f);
    CHECK(camera.scale_x>399.f && camera.scale_x<401.f);
    CHECK(camera.scale_y< -379.f && camera.scale_y> -381.f);
    // Source XY/Z can have a negative depth convention while still being
    // a coherent diagnostic candidate; never silently flip axis calibration.
    for(auto& p:source)p.source_xyz[2]=-p.source_xyz[2];
    const auto negative=FitSourcePerspective(source.data(),6,0,0,640,480);
    CHECK(negative.plausible && !negative.depth_positive);
    source[0].source_xyz[2]=5.f;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    for(auto& p:source)p.source_xyz[2]=5.f;
    source[1].source_xyz[2]=0.f;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    source[1].source_xyz[2]=5.f;
    source[0].screen_xy[0]=700.f;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    source[0].screen_xy[0]=320.f+400.f*x[0]/z[0];
    for(unsigned i=0;i<6;++i)source[i].source_xyz[2]=z[i];
    const float bad=(std::numeric_limits<float>::quiet_NaN)();
    source[2].screen_xy[0]=bad;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    source[2].screen_xy[0]=320.f+400.f*x[2]/z[2];
    CHECK(!FitSourcePerspective(source.data(),5,0,0,640,480).plausible);
    CHECK(!FitSourcePerspective(source.data(),6,0,0,0,480).plausible);
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,0).plausible);
    // No x or y depth-dependent spread: insufficient calibration geometry.
    for(auto& p:source){p.source_xyz[0]=0.f;p.source_xyz[1]=0.f;}
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    // Multi-Draw original D3D9 sources keep one fitted camera until Present.
    using namespace m2vr::vrbridge;
    OriginalProjectedCameraEpoch gate;
    const OriginalProjectionViewport vp{0u,0u,640u,480u};
    CHECK(gate.Admit(77u,0x1234u,vp,camera)==OriginalCameraPassResult::Initial);
    auto noisy=camera;
    noisy.center_x+=0.5f;noisy.scale_x+=2.f;
    CHECK(gate.Admit(77u,0x1234u,vp,noisy)==OriginalCameraPassResult::Continuation);
    CHECK(gate.Admit(78u,0x1234u,vp,camera)==OriginalCameraPassResult::FrameChanged);
    CHECK(gate.Admit(77u,0x1235u,vp,camera)==OriginalCameraPassResult::CallerChanged);
    CHECK(gate.Admit(77u,0x1234u,{0u,0u,320u,240u},camera)==OriginalCameraPassResult::ViewportChanged);
    CHECK(gate.Admit(77u,0x1234u,vp,negative)==OriginalCameraPassResult::DepthChanged);
    auto preview=camera;preview.center_x+=40.f;
    CHECK(gate.Admit(77u,0x1234u,vp,preview)==OriginalCameraPassResult::OpticsChanged);
    preview=camera;preview.scale_y*=1.2f;
    CHECK(gate.Admit(77u,0x1234u,vp,preview)==OriginalCameraPassResult::OpticsChanged);
    preview=camera;preview.scale_x=-preview.scale_x;
    CHECK(gate.Admit(77u,0x1234u,vp,preview)==OriginalCameraPassResult::OpticsChanged);
    preview=camera;preview.center_x=bad;
    CHECK(gate.Admit(77u,0x1234u,vp,preview)==OriginalCameraPassResult::Invalid);
    gate.Reset();
    CHECK(gate.Admit(78u,0x5678u,vp,negative)==OriginalCameraPassResult::Initial);
    CHECK(gate.Admit(78u,0x5678u,vp,negative)==OriginalCameraPassResult::Continuation);
    // Plain original D3D9 XYZ Draws may alter WORLD per object while the
    // camera VIEW/PROJECTION and viewport must remain fixed until Present.
    OriginalFixedCameraEpoch fixed;
    std::array<float,16> view{},proj{};
    view[0]=view[5]=view[10]=view[15]=1.f;
    proj[0]=2.f;proj[5]=2.5f;proj[10]=1.5f;proj[11]=1.f;
    proj[14]=-0.5f;
    CHECK(fixed.Admit(77u,vp,view,proj)==OriginalFixedCameraPassResult::Initial);
    CHECK(fixed.Admit(77u,vp,view,proj)==OriginalFixedCameraPassResult::Continuation);
    CHECK(fixed.Admit(78u,vp,view,proj)==OriginalFixedCameraPassResult::FrameChanged);
    CHECK(fixed.Admit(77u,{0u,0u,320u,240u},view,proj)==OriginalFixedCameraPassResult::ViewportChanged);
    auto other_view=view;other_view[12]=0.25f;
    CHECK(fixed.Admit(77u,vp,other_view,proj)==OriginalFixedCameraPassResult::ViewChanged);
    auto other_proj=proj;other_proj[0]*=1.1f;
    CHECK(fixed.Admit(77u,vp,view,other_proj)==OriginalFixedCameraPassResult::ProjectionChanged);
    other_proj=proj;other_proj[0]=bad;
    CHECK(fixed.Admit(77u,vp,view,other_proj)==OriginalFixedCameraPassResult::Invalid);
    fixed.Reset();
    CHECK(fixed.Admit(78u,vp,other_view,proj)==OriginalFixedCameraPassResult::Initial);
    // A single source-pose world frame must never combine D3D9 WVP with
    // CPU ProjectVertexToScreen or legacy native camera-space primitives.
    OriginalCameraFamilyEpoch family;
    CHECK(family.Admit(77u,OriginalCameraSourceFamily::OriginalCPUProjected));
    CHECK(family.Admit(77u,OriginalCameraSourceFamily::OriginalCPUProjected));
    CHECK(!family.Admit(77u,OriginalCameraSourceFamily::FixedFunctionD3D9));
    CHECK(!family.Admit(78u,OriginalCameraSourceFamily::OriginalCPUProjected));
    family.Reset();
    CHECK(family.Admit(78u,OriginalCameraSourceFamily::FixedFunctionD3D9));
    CHECK(!family.Admit(78u,OriginalCameraSourceFamily::Native));
    return 0;
}
