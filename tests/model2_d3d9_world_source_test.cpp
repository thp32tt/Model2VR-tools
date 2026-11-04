// Actual fixed-function Model 2 D3D9 UP source layout/format regression.
// Does not require a game, live GPU driver, headset or FFB implementation.
#include "../src/probe/d3d9_world_source.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"D3D9 world source failure line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    auto plain=DecodeWorldUpFVF(D3DFVF_XYZ|D3DFVF_TEX1,20);
    CHECK(plain.eligible && plain.uv_offset==12 && plain.stride==20);
    const auto diffuse=DecodeWorldUpFVF(
        D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1,36);
    CHECK(diffuse.eligible&&diffuse.uv_offset==28 &&
          diffuse.has_diffuse && diffuse.diffuse_offset==24);
    CHECK(!plain.has_diffuse);
    CHECK(!DecodeWorldUpFVF(D3DFVF_XYZRHW|D3DFVF_TEX1,24).eligible);
    CHECK(!DecodeWorldUpFVF(D3DFVF_XYZ|D3DFVF_TEX1,24).eligible);
    CHECK(!DecodeWorldUpFVF(D3DFVF_XYZ|D3DFVF_TEX2,28).eligible);
    CHECK(!DecodeWorldUpFVF(D3DFVF_XYZ|D3DFVF_TEX1|
                            D3DFVF_TEXCOORDSIZE1(0),16).eligible);
    CHECK(!DecodeWorldUpFVF(D3DFVF_XYZ|D3DFVF_TEX1,0).eligible);
    D3DMATRIX world{},view{};
    world._11=world._22=world._33=world._44=1.f;
    view._11=view._22=view._33=view._44=1.f;
    world._41=10.f;world._42=20.f;world._43=30.f;
    view._41=-1.f;view._42=-2.f;view._43=-3.f;
    std::array<float,5> input{1.f,2.f,3.f,0.25f,0.75f};
    WorldVertex out{};
    CHECK(DecodeWorldVertex(input.data(),sizeof(input),plain,
                            world,view,out));
    CHECK(out.view_xyz[0]==10.f && out.view_xyz[1]==20.f &&
          out.view_xyz[2]==30.f);
    CHECK(out.uv[0]==0.25f && out.uv[1]==0.75f);
    CHECK(out.diffuse_argb==0xffffffffu);
    std::array<std::uint8_t,36> diffuse_source{};
    const float diffuse_xyz[3]{1.f,2.f,3.f};
    const float diffuse_uv[2]{0.25f,0.75f};
    const std::uint32_t diffuse_color=0x80402010u;
    std::memcpy(diffuse_source.data(),diffuse_xyz,sizeof(diffuse_xyz));
    std::memcpy(diffuse_source.data()+24,&diffuse_color,4u);
    std::memcpy(diffuse_source.data()+28,diffuse_uv,sizeof(diffuse_uv));
    CHECK(DecodeWorldVertex(diffuse_source.data(),diffuse_source.size(),
                            diffuse,world,view,out));
    CHECK(out.diffuse_argb==diffuse_color &&
          out.uv[0]==0.25f && out.uv[1]==0.75f);
    CHECK(!DecodeWorldVertex(diffuse_source.data(),35,
                             diffuse,world,view,out));
    CHECK(!DecodeWorldVertex(input.data(),19,plain,world,view,out));
    CHECK(!DecodeWorldVertex(nullptr,sizeof(input),plain,world,view,out));
    world._14=0.1f;
    CHECK(!DecodeWorldVertex(input.data(),sizeof(input),plain,world,view,out));
    world._14=0.f;
    input[4]=(std::numeric_limits<float>::quiet_NaN)();
    CHECK(!DecodeWorldVertex(input.data(),sizeof(input),plain,world,view,out));
    input[4]=0.75f;
    input[3]=70000.f;
    CHECK(!DecodeWorldVertex(input.data(),sizeof(input),plain,world,view,out));
    // Admit only source-side homogeneous perspective, including genuine
    // off-axis and either D3D9 handedness. Reject common HUD/menu ortho,
    // identity, nonfinite, shear and unexpected depth conventions.
    D3DMATRIX projection{};
    projection._11=1.2f;projection._22=1.5f;
    projection._33=1.001f;projection._34=1.f;
    projection._43=-0.1001f;
    CHECK(StandardPerspectiveProjection(projection));
    projection._31=0.22f;projection._32=-0.12f;
    CHECK(StandardPerspectiveProjection(projection)); // asymmetric frustum
    projection._34=-1.f;projection._33=-1.001f;
    CHECK(StandardPerspectiveProjection(projection)); // right handed
    projection._44=1.f;projection._34=0.f;projection._33=1.f;
    CHECK(!StandardPerspectiveProjection(projection)); // orthographic
    projection._44=0.f;projection._34=1.f;projection._33=1.001f;
    projection._43=0.f;
    CHECK(!StandardPerspectiveProjection(projection)); // no depth mapping
    projection._43=-0.1001f;
    projection._12=0.5f;
    CHECK(!StandardPerspectiveProjection(projection)); // unsupported shear
    projection._12=0.f;
    projection._11=(std::numeric_limits<float>::quiet_NaN)();
    CHECK(!StandardPerspectiveProjection(projection));
    // D3D9 is little-endian BGRA in A8R8G8B8 / X8R8G8B8.
    std::array<std::uint8_t,8> bgra{3,2,1,128, 40,50,60,0};
    std::array<std::uint8_t,8> result{};
    CHECK(DecodeTextureRowRGBA(bgra.data(),result.data(),2,
                               D3DFMT_A8R8G8B8));
    CHECK(result[0]==1 && result[1]==2 && result[2]==3 &&
          result[3]==128 && result[4]==60 && result[7]==0);
    CHECK(DecodeTextureRowRGBA(bgra.data(),result.data(),2,
                               D3DFMT_X8R8G8B8));
    CHECK(result[3]==255 && result[7]==255);
    CHECK(!DecodeTextureRowRGBA(bgra.data(),result.data(),2,
                                D3DFMT_DXT1));
    CHECK(!DecodeTextureRowRGBA(bgra.data(),result.data(),0,
                                D3DFMT_A8R8G8B8));
    CHECK(!DecodeTextureRowRGBA(bgra.data(),result.data(),1025,
                                D3DFMT_A8R8G8B8));
    return 0;
}
