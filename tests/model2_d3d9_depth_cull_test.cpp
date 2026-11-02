// Source-state-backed stereo depth and cull regression on both real RGBA
// eye targets. Does not execute Model2 or require GPU/HMD.
#include "../src/vr/model2_stereo_raster.hpp"
#include "../src/probe/d3d9_sampler_states.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#define CHECK(expr) do {if(!(expr)){std::fprintf(stderr,"stereo render state failure line %d: %s\\n",__LINE__,#expr);return 1;}} while(0)
using namespace m2vr::vr;
namespace {
std::uint32_t Center(const StereoWorldRaster& surface,int eye) {
    const auto& rgba=surface.Eye(eye);
    const auto i=(std::size_t(512)*StereoWorldRaster::Width+512u)*4u;
    return (std::uint32_t(rgba[i])<<16)|
           (std::uint32_t(rgba[i+1])<<8)|rgba[i+2];
}
ClippedTexturedStereoBatch Triangle(float depth) {
    ClippedTexturedStereoBatch batch{};
    batch.game_frame=42;batch.pose_sequence=7;
    for(auto& eye:batch.eyes) {
        ClippedTexturedEyeTriangle t{};
        t.vertices[0]={{-0.5f,-0.5f,depth,true},0.f,0.f};
        t.vertices[1]={{ 0.5f,-0.5f,depth,true},1.f,0.f};
        t.vertices[2]={{ 0.0f, 0.5f,depth,true},0.f,1.f};
        eye.push_back(t);
    }
    return batch;
}
void CheckBoth(const StereoWorldRaster& raster,std::uint32_t rgb,bool& ok){
    for(int eye=0;eye<2;++eye)
        if(Center(raster,eye)!=rgb)ok=false;
}
}
int main() {
    CHECK(TriangleSurvivesCull(-1.,FaceCull::Clockwise));
    CHECK(!TriangleSurvivesCull(-1.,FaceCull::CounterClockwise));
    CHECK(TriangleSurvivesCull(1.,FaceCull::CounterClockwise));
    CHECK(!TriangleSurvivesCull(1.,FaceCull::Clockwise));
    CHECK(!TriangleSurvivesCull(0.,FaceCull::None));
    CHECK(DepthPasses(3.,3.f,DepthCompare::LessEqual));
    CHECK(!DepthPasses(3.,3.f,DepthCompare::Less));
    CHECK(!DepthPasses(-1.,1.f,DepthCompare::Less));
    CHECK(!ValidRasterPolicy({
        static_cast<FaceCull>(5),DepthCompare::Less,true}));

    using m2vr::drawprobe::DecodeRasterPolicy;
    RasterPolicy decoded{};
    constexpr DWORD colors=D3DCOLORWRITEENABLE_RED|
        D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|
        D3DCOLORWRITEENABLE_ALPHA;
    CHECK(DecodeRasterPolicy(D3DCULL_CW,D3DCMP_LESSEQUAL,0,
                             0,0,0,0,colors,decoded));
    CHECK(decoded.cull==FaceCull::Clockwise &&
          decoded.depth==DepthCompare::LessEqual &&
          !decoded.write_depth);
    CHECK(!DecodeRasterPolicy(D3DCULL_CW,D3DCMP_ALWAYS,1,
                              0,0,0,0,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_CW,D3DCMP_LESS,1,
                              1,0,0,0,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_CCW,D3DCMP_LESS,1,
                              0,0x3f800000u,0,0,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_NONE,D3DCMP_LESS,1,
                              0,0,0,1,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_NONE,D3DCMP_LESS,1,
                              0,0,0,0,D3DCOLORWRITEENABLE_RED,decoded));

    const std::array<std::uint8_t,4> red={255,0,0,255};
    const std::array<std::uint8_t,4> blue={0,0,255,255};
    StereoWorldRaster::TextureRGBA r{red.data(),1,1,4,0};
    StereoWorldRaster::TextureRGBA b{blue.data(),1,1,4,0};
    const auto near_tri=Triangle(2.f);
    const auto farther=Triangle(3.f);
    StereoWorldRaster raster;
    bool ok=true;
    r.raster.cull=FaceCull::CounterClockwise; // culls screen-CCW tri
    CHECK(raster.DrawTextured(farther,r));
    CheckBoth(raster,0x000000u,ok);CHECK(ok);
    r.raster.cull=FaceCull::Clockwise;
    CHECK(raster.DrawTextured(farther,r));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    // A second coplanar draw cannot win LESS, but must win LESS_EQUAL.
    CHECK(raster.DrawTextured(farther,b));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    b.raster.depth=DepthCompare::LessEqual;
    CHECK(raster.DrawTextured(farther,b));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    // A nearer object with disabled Z writes renders but does not occlude
    // a subsequently drawn farther source triangle.
    raster.Clear();
    b.raster.write_depth=false;
    CHECK(raster.DrawTextured(near_tri,b));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    r.raster.cull=FaceCull::None;
    CHECK(raster.DrawTextured(farther,r));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    // Restore depth writes: now a far draw cannot replace the near draw.
    raster.Clear();
    b.raster.write_depth=true;
    CHECK(raster.DrawTextured(near_tri,b));
    CHECK(raster.DrawTextured(farther,r));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    // Unsupported state fails before modifying either target.
    auto invalid=r;
    invalid.raster.cull=static_cast<FaceCull>(77);
    const auto old=raster.DrawnPixels();
    CHECK(!raster.DrawTextured(farther,invalid));
    CHECK(raster.DrawnPixels()==old);
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    return 0;
}
