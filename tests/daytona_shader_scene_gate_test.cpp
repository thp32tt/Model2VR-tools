#include "../src/probe/daytona_shader_scene_gate.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"shader gate FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace m2vr::daytona;
static ShaderCandidateState Valid(){
    ShaderCandidateState s{};
    s.z_enable=D3DZB_TRUE;s.z_write=TRUE;s.z_func=D3DCMP_LESSEQUAL;
    s.cull=D3DCULL_CCW;
    s.color_mask=D3DCOLORWRITEENABLE_RED|
                 D3DCOLORWRITEENABLE_GREEN|
                 D3DCOLORWRITEENABLE_BLUE|
                 D3DCOLORWRITEENABLE_ALPHA;
    return s;
}
int main(){
    const D3DVIEWPORT9 full{0,0,640,480,0.f,1.f};
    const auto accepted=Valid();
    CHECK(ClassifyShaderUPCandidate(accepted,full,640,480)==ShaderCandidateReject::None);
    auto s=accepted;s.z_func=D3DCMP_LESS;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::None);
    s=accepted;s.z_enable=D3DZB_FALSE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Depth);
    s=accepted;s.z_write=FALSE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Depth);
    s=accepted;s.z_func=D3DCMP_ALWAYS;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Depth);
    s=accepted;s.alpha_blend=TRUE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::BlendAlpha);
    s=accepted;s.alpha_test=TRUE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::BlendAlpha);
    s=accepted;s.stencil=TRUE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Stencil);
    s=accepted;s.scissor=TRUE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Scissor);
    s=accepted;s.color_mask=D3DCOLORWRITEENABLE_RED;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::WriteMask);
    s=accepted;s.depth_bias=0x3f800000u;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Bias);
    s=accepted;s.slope_bias=0x3f800000u;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Bias);
    s=accepted;s.clip_plane_enable=1;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::ClipPlane);
    s=accepted;s.cull=0;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Cull);
    s=accepted;s.srgb_write=TRUE;
    CHECK(ClassifyShaderUPCandidate(s,full,640,480)==ShaderCandidateReject::Srgb);
    D3DVIEWPORT9 cropped=full;cropped.X=5;
    CHECK(ClassifyShaderUPCandidate(accepted,cropped,640,480)==ShaderCandidateReject::Viewport);
    cropped=full;cropped.MinZ=0.1f;
    CHECK(ClassifyShaderUPCandidate(accepted,cropped,640,480)==ShaderCandidateReject::Viewport);
    cropped=full;cropped.Width=320;
    CHECK(ClassifyShaderUPCandidate(accepted,cropped,640,480)==ShaderCandidateReject::Viewport);
    CHECK(ClassifyShaderUPCandidate(accepted,full,0,480)==ShaderCandidateReject::Viewport);
    CHECK(CheckShaderUPCandidate(nullptr,full,640,480)==ShaderCandidateReject::DeviceQuery);
    return 0;
}
