#include "../src/probe/projected_up_world_source.hpp"
#include "../src/probe/project_draw_correlator.hpp"
#include "../src/probe/model2_camera_projection_fit.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"original projected UP world %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    constexpr auto fvf=D3DFVF_XYZRHW|D3DFVF_TEX1;
    CHECK(ExactProjectedUPWorldFVF(fvf,24u));
    CHECK(!ExactProjectedUPWorldFVF(fvf,28u));
    CHECK(!ExactProjectedUPWorldFVF(D3DFVF_XYZ|D3DFVF_TEX1,24u));
    CHECK(ExactProjectedUPWorldFVF(fvf|D3DFVF_DIFFUSE,28u));
    CHECK(!ExactProjectedUPWorldFVF(fvf|D3DFVF_DIFFUSE,24u));
    CHECK(!ExactProjectedUPWorldFVF(fvf|D3DFVF_SPECULAR,28u));
    CHECK(!ExactProjectedUPWorldFVF(fvf|D3DFVF_DIFFUSE|D3DFVF_SPECULAR,32u));
    const std::array<float,6> packed{120.f,140.f,0.5f,0.25f,0.4f,0.7f};
    ProjectedUPWorldVertex v{};
    CHECK(DecodeProjectedUPWorldVertex(packed.data(),fvf,24,v));
    CHECK(v.screen_xy[0]==120.f && v.uv[1]==0.7f);
    CHECK(!v.has_diffuse && v.diffuse_argb==0xffffffffu);
    // Original D3DCOLOR occupies four raw bytes between XYZRHW and UV.
    // The 0xff... pattern must NOT be interpreted as a NaN float.
    std::array<std::uint8_t,28> with_diffuse{};
    const std::array<float,4> rhw{120.f,140.f,0.5f,0.25f};
    const std::array<float,2> colored_uv{0.4f,0.7f};
    const std::uint32_t argb=0xff804020u;
    std::memcpy(with_diffuse.data(),rhw.data(),16u);
    std::memcpy(with_diffuse.data()+16u,&argb,4u);
    std::memcpy(with_diffuse.data()+20u,colored_uv.data(),8u);
    CHECK(DecodeProjectedUPWorldVertex(with_diffuse.data(),
        fvf|D3DFVF_DIFFUSE,28u,v));
    CHECK(v.has_diffuse && v.diffuse_argb==argb &&
          v.screen_xy[0]==120.f && v.uv[1]==0.7f);
    CHECK(!DecodeProjectedUPWorldVertex(with_diffuse.data(),
        fvf|D3DFVF_DIFFUSE,24u,v));
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    std::memcpy(with_diffuse.data()+20u,&nan,sizeof(nan));
    CHECK(!DecodeProjectedUPWorldVertex(with_diffuse.data(),
        fvf|D3DFVF_DIFFUSE,28u,v));
    CHECK(!DecodeProjectedUPWorldVertex(nullptr,fvf,24,v));
    CHECK(!DecodeProjectedUPWorldVertex(packed.data(),fvf,32,v));
    auto wrong=packed;wrong[3]=0.f;
    CHECK(!DecodeProjectedUPWorldVertex(wrong.data(),fvf,24,v));
    wrong=packed;wrong[2]=1.5f;
    CHECK(!DecodeProjectedUPWorldVertex(wrong.data(),fvf,24,v));
    // Strict same render-thread/Present source ownership before an
    // independently authenticated adapter could use original UP input.
    ProjectDrawCorrelator ring{};
    std::array<ProjectedDrawVertex,6> screen{};
    for(std::size_t i=0;i<6u;++i){
        const float x=(i%3u==0u?-0.3f:i%3u==1u?0.2f:0.1f)+
                      (i>=3u?0.1f:0.f);
        const float y=(i%3u==0u?-0.1f:i%3u==1u?0.2f:-0.3f)+
                      (i>=3u?0.15f:0.f);
        const float z=3.f+float(i);
        const float src[3]{x*z,y*z,z};
        const float dst[3]{320.f+350.f*x,240.f-350.f*y,0.5f};
        screen[i]={dst[0],dst[1]};
        ring.Capture(5u,0x222u,src,dst);
    }
    const auto full=ring.MatchTriangleList(5u,screen.data(),6u);
    CHECK(full.CompleteSameSource());
    CHECK(full.camera_sample_count==6u);
    CHECK(full.common_source_caller_rva==0x222u);
    const auto camera=FitSourcePerspective(
        full.camera_samples.data(),full.camera_sample_count,
        0,0,640,480);
    CHECK(camera.plausible && camera.rms_pixels<0.001f);
    CHECK(!ring.MatchTriangleList(6u,screen.data(),6u).CompleteSameSource());
    ring.Capture(5u,0x222u,
        std::array<float,3>{1.f,2.f,3.f}.data(),
        std::array<float,3>{screen[0].x,screen[0].y,0.5f}.data());
    CHECK(!ring.MatchTriangleList(5u,screen.data(),6u).CompleteSameSource());
    return 0;
}
