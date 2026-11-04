// Source-state-backed stereo depth and cull regression on both real RGBA
// eye targets. Does not execute Model2 or require GPU/HMD.
#include "../src/vr/model2_stereo_raster.hpp"
#include "../src/probe/d3d9_sampler_states.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>
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
    // Real perspective interpolation often gives a double that rounds
    // DOWN when stored in our float depth surface. LEQUAL must still
    // accept the identical next pass; LESS must reject it.
    const double source_z=1.0/(0.24951171875+0.75048828125/16.0);
    const float saved_z=static_cast<float>(source_z);
    CHECK(source_z>double(saved_z));
    CHECK(DepthPasses(source_z,saved_z,DepthCompare::LessEqual));
    CHECK(!DepthPasses(source_z,saved_z,DepthCompare::Less));
    CHECK(!DepthPasses(3.,3.f,DepthCompare::Less));
    CHECK(DepthPasses(5.,3.f,DepthCompare::Always));
    CHECK(!DepthPasses(-1.,3.f,DepthCompare::Always));
    CHECK(!DepthPasses(-1.,1.f,DepthCompare::Less));
    // Compare against float-quantized original Z for the new source modes.
    CHECK(DepthPasses(source_z,saved_z,DepthCompare::Equal));
    CHECK(!DepthPasses(source_z,saved_z,DepthCompare::NotEqual));
    CHECK(DepthPasses(4.,saved_z,DepthCompare::Greater));
    CHECK(DepthPasses(4.,saved_z,DepthCompare::GreaterEqual));
    CHECK(DepthPasses(source_z,saved_z,DepthCompare::GreaterEqual));
    CHECK(!DepthPasses(source_z,saved_z,DepthCompare::Greater));
    CHECK(!DepthPasses(2.,3.f,DepthCompare::Greater));
    CHECK(DepthPasses(2.,3.f,DepthCompare::NotEqual));
    CHECK(!DepthPasses(-1.,3.f,DepthCompare::NotEqual));
    CHECK(!ValidRasterPolicy({
        FaceCull::None,static_cast<DepthCompare>(7),true}));
    CHECK(!ValidRasterPolicy({
        static_cast<FaceCull>(5),DepthCompare::Less,true}));

    // Genuine original shader/clip state is independently required.
    // A well-formed FVF alone can NEVER certify Gouraud interpolation,
    // original clipping or CPU world-space ownership.
    using m2vr::drawprobe::OriginalFilledTriangleEquivalent;
    CHECK(OriginalFilledTriangleEquivalent(D3DFILL_SOLID));
    CHECK(!OriginalFilledTriangleEquivalent(D3DFILL_WIREFRAME));
    CHECK(!OriginalFilledTriangleEquivalent(D3DFILL_POINT));
    CHECK(!OriginalFilledTriangleEquivalent(0u));
    using m2vr::drawprobe::OriginalVertexPipelineEquivalent;
    CHECK(OriginalVertexPipelineEquivalent(
        D3DSHADE_GOURAUD,TRUE,0u,D3DVBF_DISABLE,FALSE));
    CHECK(!OriginalVertexPipelineEquivalent(
        D3DSHADE_FLAT,TRUE,0u,D3DVBF_DISABLE,FALSE));
    CHECK(!OriginalVertexPipelineEquivalent(
        D3DSHADE_PHONG,TRUE,0u,D3DVBF_DISABLE,FALSE));
    CHECK(!OriginalVertexPipelineEquivalent(
        D3DSHADE_GOURAUD,FALSE,0u,D3DVBF_DISABLE,FALSE));
    CHECK(!OriginalVertexPipelineEquivalent(
        D3DSHADE_GOURAUD,TRUE,1u,D3DVBF_DISABLE,FALSE));
    CHECK(!OriginalVertexPipelineEquivalent(
        D3DSHADE_GOURAUD,TRUE,0u,D3DVBF_1WEIGHTS,FALSE));
    CHECK(!OriginalVertexPipelineEquivalent(
        D3DSHADE_GOURAUD,TRUE,0u,D3DVBF_DISABLE,TRUE));

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
    // D3DCMP_ALWAYS bypasses source depth comparison but still honors
    // ZWRITEENABLE and the exact original color and cull state.
    CHECK(DecodeRasterPolicy(D3DCULL_CW,D3DCMP_ALWAYS,1,
                              0,0,0,0,colors,decoded));
    CHECK(decoded.depth==DepthCompare::Always && decoded.write_depth);
    const struct {DWORD original;DepthCompare expected;} other_depth[]{
        {D3DCMP_EQUAL,DepthCompare::Equal},
        {D3DCMP_GREATER,DepthCompare::Greater},
        {D3DCMP_GREATEREQUAL,DepthCompare::GreaterEqual},
        {D3DCMP_NOTEQUAL,DepthCompare::NotEqual}
    };
    for(const auto& mode:other_depth){
        CHECK(DecodeRasterPolicy(D3DCULL_NONE,mode.original,1,
                                 0,0,0,0,colors,decoded));
        CHECK(decoded.depth==mode.expected && decoded.write_depth);
    }
    CHECK(!DecodeRasterPolicy(D3DCULL_CW,D3DCMP_NEVER,1,
                              0,0,0,0,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_CW,D3DCMP_LESS,1,
                              1,0,0,0,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_CCW,D3DCMP_LESS,1,
                              0,0x3f800000u,0,0,colors,decoded));
    CHECK(!DecodeRasterPolicy(D3DCULL_NONE,D3DCMP_LESS,1,
                              0,0,0,1,colors,decoded));
    CHECK(DecodeRasterPolicy(D3DCULL_NONE,D3DCMP_LESS,1,
                              0,0,0,0,D3DCOLORWRITEENABLE_RED,decoded));
    CHECK(decoded.write_rgba_mask==D3DCOLORWRITEENABLE_RED);
    CHECK(DecodeRasterPolicy(D3DCULL_NONE,D3DCMP_LESS,1,
                              0,0,0,0,0u,decoded));
    CHECK(decoded.write_rgba_mask==0u);
    CHECK(!DecodeRasterPolicy(D3DCULL_NONE,D3DCMP_LESS,1,
                              0,0,0,0,0x10u,decoded));

    // Strict original D3D9 BLENDOP_ADD classification; no approximation.
    using m2vr::drawprobe::DecodeBlendPolicy;
    BlendMode decoded_blend{};
    CHECK(DecodeBlendPolicy(0u,99,99,99,99,decoded_blend));
    CHECK(decoded_blend==BlendMode::Opaque);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_SRCALPHA,
        D3DBLEND_INVSRCALPHA,D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::SourceAlphaOver);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_ONE,D3DBLEND_ONE,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::Additive);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_ONE,D3DBLEND_ZERO,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::ReplaceRGBA);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_SRCALPHA,D3DBLEND_ONE,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::SourceAlphaAdd);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_ONE,D3DBLEND_INVSRCALPHA,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::PremultipliedOver);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_DESTCOLOR,D3DBLEND_ZERO,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::MultiplyColor);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_ZERO,D3DBLEND_SRCCOLOR,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::MultiplyColor);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_DESTALPHA,
        D3DBLEND_INVDESTALPHA,D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::DestinationAlphaOver);
    CHECK(DecodeBlendPolicy(1,D3DBLEND_ZERO,D3DBLEND_ONE,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(decoded_blend==BlendMode::PreserveDestination);
    CHECK(!DecodeBlendPolicy(1,D3DBLEND_DESTALPHA,D3DBLEND_ONE,
        D3DBLENDOP_ADD,0,decoded_blend));
    CHECK(!DecodeBlendPolicy(1,D3DBLEND_ONE,D3DBLEND_ONE,
        D3DBLENDOP_REVSUBTRACT,0,decoded_blend));
    CHECK(!DecodeBlendPolicy(1,D3DBLEND_ONE,D3DBLEND_ONE,
        D3DBLENDOP_ADD,1,decoded_blend));

    // Only the original game's compatible main depth-stencil surface
    // may be used as provenance for a projected binocular world draw.
    D3DSURFACE_DESC main_color{}, main_depth{};
    main_color.Width=640;main_color.Height=480;
    main_color.MultiSampleType=D3DMULTISAMPLE_4_SAMPLES;
    main_color.MultiSampleQuality=1u;
    main_depth=main_color;
    main_depth.Usage=D3DUSAGE_DEPTHSTENCIL;
    CHECK(m2vr::drawprobe::CompatibleOriginalDepthSurface(
        main_color,main_depth));
    main_depth.Height=479u;
    CHECK(!m2vr::drawprobe::CompatibleOriginalDepthSurface(
        main_color,main_depth));
    main_depth.Height=480u;main_depth.MultiSampleQuality=0u;
    CHECK(!m2vr::drawprobe::CompatibleOriginalDepthSurface(
        main_color,main_depth));
    main_depth.MultiSampleQuality=1u;main_depth.Usage=0u;
    CHECK(!m2vr::drawprobe::CompatibleOriginalDepthSurface(
        main_color,main_depth));
    CHECK(m2vr::drawprobe::EmptyExtraRenderTarget(
        D3DERR_NOTFOUND,false));
    CHECK(m2vr::drawprobe::EmptyExtraRenderTarget(S_OK,false));
    CHECK(!m2vr::drawprobe::EmptyExtraRenderTarget(S_OK,true));
    CHECK(!m2vr::drawprobe::EmptyExtraRenderTarget(E_FAIL,false));

    // Source alpha test equivalences are evaluated on integer RGBA8
    // texels before either independent-eye blend/depth write.
    using m2vr::drawprobe::AlphaCutoffMatches;
    CHECK(AlphaCutoffMatches(0u, D3DCMP_NEVER, 0u, 0u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_GREATEREQUAL, 127u, 127u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_GREATER, 127u, 128u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_GREATER, 0u, 1u));
    CHECK(!AlphaCutoffMatches(1u, D3DCMP_GREATER, 255u, 255u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_NOTEQUAL, 0u, 1u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_EQUAL, 255u, 255u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_ALWAYS, 17u, 0u));
    CHECK(AlphaCutoffMatches(1u, D3DCMP_LESSEQUAL, 255u, 0u));
    CHECK(!AlphaCutoffMatches(1u, D3DCMP_LESSEQUAL, 254u, 0u));
    CHECK(!AlphaCutoffMatches(1u, D3DCMP_NOTEQUAL, 17u, 18u));
    CHECK(!AlphaCutoffMatches(1u, D3DCMP_EQUAL, 128u, 128u));
    CHECK(!AlphaCutoffMatches(1u, D3DCMP_GREATER, 127u, 127u));

    // The independently rendered game-world target cannot honor an
    // original sub-viewport, cropped scissor or depth-range remap.
    D3DVIEWPORT9 viewport{};
    viewport.X=0;viewport.Y=0;
    viewport.Width=640;viewport.Height=480;
    viewport.MinZ=0.f;viewport.MaxZ=1.f;
    D3DSURFACE_DESC backbuffer{};
    backbuffer.Width=640;backbuffer.Height=480;
    CHECK(m2vr::drawprobe::FullBackbufferViewport(
        0u,viewport,backbuffer));
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        1u,viewport,backbuffer));
    // D3D9 full-size scissor is an exact no-op, but a partial or
    // unknown scissor must never contaminate independently shifted eyes.
    RECT full_scissor{0,0,640,480};
    CHECK(m2vr::drawprobe::FullBackbufferViewport(
        1u,viewport,backbuffer,&full_scissor));
    RECT cropped{1,0,640,480};
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        1u,viewport,backbuffer,&cropped));
    cropped={0,0,639,480};
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        1u,viewport,backbuffer,&cropped));
    cropped={-1,0,640,480};
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        1u,viewport,backbuffer,&cropped));
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        1u,viewport,backbuffer,nullptr));
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        2u,viewport,backbuffer,&full_scissor));
    viewport.X=1;
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        0u,viewport,backbuffer));
    viewport.X=0;viewport.Width=639;
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        0u,viewport,backbuffer));
    viewport.Width=640;viewport.MaxZ=0.75f;
    CHECK(!m2vr::drawprobe::FullBackbufferViewport(
        0u,viewport,backbuffer));
    viewport.MaxZ=1.f;
    CHECK(m2vr::drawprobe::FullBackbufferViewport(
        0u,viewport,backbuffer));

    const std::array<std::uint8_t,4> red={255,0,0,255};
    const std::array<std::uint8_t,4> blue={0,0,255,255};
    StereoWorldRaster::TextureRGBA r{red.data(),1,1,4,0};
    StereoWorldRaster::TextureRGBA b{blue.data(),1,1,4,0};
    const auto near_tri=Triangle(2.f);
    const auto farther=Triangle(3.f);
    StereoWorldRaster raster;
    bool ok=true;
    // Reverse the same original game triangle's screen-space winding.
    // One cull mode must reject it and the opposite mode must accept it,
    // independently for left and right eye RGBA buffers.
    auto flipped=farther;
    for(auto& eye:flipped.eyes)
        std::swap(eye[0].vertices[1],eye[0].vertices[2]);
    r.raster.cull=FaceCull::Clockwise;
    CHECK(raster.DrawTextured(flipped,r));
    CheckBoth(raster,0x000000u,ok);CHECK(ok);
    r.raster.cull=FaceCull::CounterClockwise;
    CHECK(raster.DrawTextured(flipped,r));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    raster.Clear();
    r.raster.cull=FaceCull::None;
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
    // Authenticated ZFUNC=ALWAYS writes color behind a nearer Z,
    // independently preserving or updating the source Z-write state.
    raster.Clear();
    CHECK(raster.DrawTextured(near_tri,b));
    r.raster.depth=DepthCompare::Always;
    r.raster.write_depth=false;
    CHECK(raster.DrawTextured(farther,r));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    r.raster.write_depth=true;
    CHECK(raster.DrawTextured(farther,r));
    b.raster.depth=DepthCompare::Less;
    CHECK(raster.DrawTextured(near_tri,b));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    r.raster.depth=DepthCompare::Less;
    // Restore depth writes: now a far draw cannot replace the near draw.
    raster.Clear();
    b.raster.write_depth=true;
    CHECK(raster.DrawTextured(near_tri,b));
    CHECK(raster.DrawTextured(farther,r));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);

    // Real independently rendered left/right Z buffers honor the extended
    // original source compares; read-only source authentication remains required.
    auto depth_base=b;
    depth_base.raster.depth=DepthCompare::Less;
    depth_base.raster.write_depth=true;
    auto depth_overlay=r;
    depth_overlay.raster.depth=DepthCompare::Equal;
    depth_overlay.raster.write_depth=false;
    raster.Clear();
    CHECK(raster.DrawTextured(near_tri,depth_base));
    CHECK(raster.DrawTextured(farther,depth_overlay));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    CHECK(raster.DrawTextured(near_tri,depth_overlay));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    raster.Clear();
    depth_overlay.raster.depth=DepthCompare::NotEqual;
    CHECK(raster.DrawTextured(near_tri,depth_base));
    CHECK(raster.DrawTextured(near_tri,depth_overlay));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    CHECK(raster.DrawTextured(farther,depth_overlay));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    raster.Clear();
    depth_overlay.raster.depth=DepthCompare::Greater;
    CHECK(raster.DrawTextured(near_tri,depth_base));
    CHECK(raster.DrawTextured(near_tri,depth_overlay));
    CheckBoth(raster,0x0000FFu,ok);CHECK(ok);
    CHECK(raster.DrawTextured(farther,depth_overlay));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    raster.Clear();
    depth_overlay.raster.depth=DepthCompare::GreaterEqual;
    CHECK(raster.DrawTextured(near_tri,depth_base));
    CHECK(raster.DrawTextured(near_tri,depth_overlay));
    CheckBoth(raster,0xFF0000u,ok);CHECK(ok);
    CHECK(!raster.StereoPairUsable()); // identical synthetic eye geometry
    // Exact source color masks preserve disabled destination channels
    // (including alpha) in BOTH verified binocular eye render targets.
    raster.Clear();
    StereoWorldRaster::TextureRGBA base{red.data(),1,1,4,0};
    CHECK(raster.DrawTextured(farther,base));
    const std::array<std::uint8_t,4> gsrc={0u,255u,0u,64u};
    StereoWorldRaster::TextureRGBA masked{gsrc.data(),1,1,4,0};
    masked.raster.depth=DepthCompare::LessEqual;
    masked.raster.write_rgba_mask=0x0au; // green, alpha
    CHECK(raster.DrawTextured(farther,masked));
    for(int eye=0;eye<2;++eye){
        CHECK(Center(raster,eye)==0xffff00u);
        const auto& rgba=raster.Eye(eye);
        const auto pixel=(std::size_t(512)*StereoWorldRaster::Width+512u)*4u;
        CHECK(rgba[pixel+3u]==64u);
    }
    masked.rgba=blue.data();
    masked.blend=BlendMode::SourceAlphaOver;
    masked.raster.write_rgba_mask=0x05u; // red, blue only
    CHECK(raster.DrawTextured(farther,masked));
    for(int eye=0;eye<2;++eye){
        CHECK(Center(raster,eye)==0x00ffffu); // green retained
        const auto& rgba=raster.Eye(eye);
        const auto pixel=(std::size_t(512)*StereoWorldRaster::Width+512u)*4u;
        CHECK(rgba[pixel+3u]==64u); // alpha retained
    }
    // Z-only Draw must occlude future color but never make stereo usable.
    raster.Clear();
    auto z_only=base;
    z_only.raster.write_rgba_mask=0u;
    CHECK(raster.DrawTextured(near_tri,z_only));
    CHECK(raster.DrawnPixels()==0u && !raster.StereoPairUsable());
    CHECK(raster.DrawTextured(farther,base));
    CheckBoth(raster,0x000000u,ok);CHECK(ok);
    CHECK(!raster.StereoPairUsable());
    // Alpha-only output does change RGBA but must NEVER certify a
    // distinct RGB eye pair for the x86->x64 OpenXR IPC bridge.
    raster.Clear();
    const std::array<std::uint8_t,4> black={0u,0u,0u,255u};
    StereoWorldRaster::TextureRGBA rgb_black{black.data(),1,1,4,0};
    rgb_black.raster.write_rgba_mask=0x07u;
    CHECK(raster.DrawTextured(farther,rgb_black));
    CHECK(raster.DrawnPixels()>0u && !raster.StereoPairUsable());
    auto left_alpha=near_tri;
    left_alpha.eyes[1].clear();
    StereoWorldRaster::TextureRGBA alpha_only{gsrc.data(),1,1,4,0};
    alpha_only.raster.write_rgba_mask=0x08u;
    CHECK(raster.DrawTextured(left_alpha,alpha_only));
    CHECK(raster.Eye(0)!=raster.Eye(1)); // differing alpha ONLY
    CHECK(!raster.StereoPairUsable());
    // Disabled RGB channels may still write Z, but alpha pixels alone
    // do not increment per-eye native stereo color-coverage counters.
    raster.Clear();
    CHECK(raster.DrawTextured(near_tri,alpha_only));
    CHECK(raster.DrawnPixels()==0u && !raster.StereoPairUsable());
    // Unsupported state fails before modifying either target.
    auto invalid=r;
    invalid.raster.cull=static_cast<FaceCull>(77);
    const auto old=raster.DrawnPixels();
    CHECK(!raster.DrawTextured(farther,invalid));
    CHECK(raster.DrawnPixels()==old);
    // Alpha-only prior Draw leaves the clear RGB black, not the blue
    // that an earlier independent test section wrote before Clear().
    CheckBoth(raster,0x000000u,ok);CHECK(ok);
    return 0;
}
