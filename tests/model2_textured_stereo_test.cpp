#include "../src/vr/model2_stereo_raster.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"textured stereo fail %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr;
using namespace m2vr::vr;

static std::uint32_t Pixel(const StereoWorldRaster& r,int eye,int x,int y){
    const auto& p=r.Eye(eye);
    const auto i=(std::size_t(y)*StereoWorldRaster::Width+std::size_t(x))*4;
    return (std::uint32_t(p[i])<<16)|(std::uint32_t(p[i+1])<<8)|p[i+2];
}
int main(){
    DrawEvidence world{RenderPass::World3D,true,true,true,
        false,false,false,false,true,true,true};
    Model2CameraCalibration cal{};
    cal.view_to_xr={1,0,0,0,1,0,0,0,-1};
    cal.units_per_meter=1.f;cal.min_depth_m=0.05f;
    cal.runtime_confirmed=true;
    pose::Frame pose{};
    pose.sequence=27;
    pose.view_flags=pose::kOrientationValid|pose::kPositionValid;
    for(int i=0;i<2;++i){
        pose.eyes[i].position_m[0]=i?0.04f:-0.04f;
        pose.eyes[i].orientation_xyzw[3]=1.f;
        auto& f=pose.eyes[i].fov_left_right_up_down;
        f[0]=-0.78539816f;f[1]=0.78539816f;
        f[2]=0.78539816f;f[3]=-0.78539816f;
    }
    Model2FramePoseLatch latch;
    CHECK(latch.Begin(100,pose));
    const std::array<CpuPoint,3> near_vertices{{{-0.5f,-0.5f,2.f},
         {0.5f,-0.5f,2.f},{0.f,0.5f,2.f}}};
    const std::array<CpuPoint,3> far_vertices{{{-1.f,-1.f,4.f},
         {1.f,-1.f,4.f},{0.f,1.f,4.f}}};
    const std::uint32_t indices[]={0,1,2};
    const float uv[]={0.f,1.f, 1.f,1.f, 0.5f,0.f};
    ClippedTexturedStereoBatch textured{};
    CHECK(BuildTexturedStereoBatch(near_vertices.data(),uv,3,indices,3,
                                   100,latch,cal,world,textured));
    CHECK(textured.game_frame==100 && textured.pose_sequence==27);
    CHECK(textured.eyes[0].size()==1 && textured.eyes[1].size()==1);
    CHECK(textured.eyes[0][0].vertices[0].position.ndc_x >
          textured.eyes[1][0].vertices[0].position.ndc_x);
    // Actual D3DCOLOR argb is decoded into independent linear RGBA
    // source attributes and preserved across both asymmetric eye clips.
    const std::uint32_t diffuse_argb[]={0x80402010u,0xff00ff00u,0xffff0000u};
    ClippedTexturedStereoBatch diffuse_batch{};
    CHECK(BuildTexturedStereoBatch(near_vertices.data(),uv,3,indices,3,
        100,latch,cal,world,diffuse_batch,diffuse_argb));
    for(const auto& eye:diffuse_batch.eyes){
        CHECK(!eye.empty());
        const auto& first=eye.front().vertices[0].diffuse_rgba;
        CHECK(std::abs(first[0]-64.f/255.f)<0.0001f);
        CHECK(std::abs(first[1]-32.f/255.f)<0.0001f);
        CHECK(std::abs(first[2]-16.f/255.f)<0.0001f);
        CHECK(std::abs(first[3]-128.f/255.f)<0.0001f);
    }
    std::array<std::uint8_t,64> tex{};
    for(int y=0;y<4;++y)for(int x=0;x<4;++x){
        auto p=(y*4+x)*4;
        tex[p]=x<2?255:0;
        tex[p+1]=x<2?0:255;
        tex[p+2]=0;
        tex[p+3]=(x==2 && y==2)?0:255;
    }
    StereoWorldRaster::TextureRGBA image{tex.data(),4,4,16,128};
    StereoWorldRaster raster;
    const std::uint32_t blue[]={0x2040CD};
    ClippedStereoBatch background{};
    CHECK(BuildClippedStereoBatch(far_vertices.data(),3,indices,3,
                                  100,latch,cal,world,background));
    CHECK(raster.Draw(background,blue,1));
    const auto count_before=raster.DrawnPixels();
    CHECK(raster.DrawTextured(textured,image));
    CHECK(raster.DrawnPixels()>count_before);
    // Different per-eye geometry; both see the same source texture.
    std::uint64_t red[2]{},green[2]{},background_visible[2]{};
    double colored_centroid[2]{};
    for(int eye=0;eye<2;++eye){
        for(int y=384;y<641;++y)for(int x=384;x<641;++x){
            const auto p=Pixel(raster,eye,x,y);
            if(p==0xFF0000){++red[eye];colored_centroid[eye]+=x;}
            if(p==0x00FF00){++green[eye];colored_centroid[eye]+=x;}
            if(p==blue[0])++background_visible[eye];
        }
        CHECK(red[eye]>150);
        CHECK(green[eye]>150);
        CHECK(background_visible[eye]>100);
        colored_centroid[eye]/=double(red[eye]+green[eye]);
    }
    CHECK(colored_centroid[0]>colored_centroid[1]+10.0);
    // Specifically test the zero-alpha texel at the *different* eye-space
    // centers. Opaque background must remain visible, without a new Z write.
    CHECK(Pixel(raster,0,522,512)==blue[0]);
    CHECK(Pixel(raster,1,502,512)==blue[0]);

    // One eye can clip the textured source into a different fan.
    const std::array<CpuPoint,3> near_cross{{
        {-0.3f,-0.3f,1.f},{0.3f,-0.3f,1.f},{0.f,0.3f,0.01f}}};
    ClippedTexturedStereoBatch near_clipped{};
    CHECK(BuildTexturedStereoBatch(near_cross.data(),uv,3,indices,3,
                                  100,latch,cal,world,near_clipped));
    CHECK(!near_clipped.eyes[0].empty()&&!near_clipped.eyes[1].empty());
    for(const auto& eye:near_clipped.eyes)
        for(const auto& t:eye)
            for(const auto& v:t.vertices)
                CHECK(v.position.depth_m>=cal.min_depth_m-1.e-5f &&
                      std::isfinite(v.u)&&std::isfinite(v.v));
    // Perspective-correct UV: at center, near vertex with u=0 dominates
    // the two 4m u=1 vertices. Affine would sample a different texel.
    raster.Clear();
    ClippedTexturedStereoBatch manual{};
    manual.game_frame=100;manual.pose_sequence=27;
    for(auto& eye:manual.eyes){
        ClippedTexturedEyeTriangle tri{};
        tri.vertices[0]={{-0.5f,-0.5f,1.f,true},0.f,0.f};
        tri.vertices[1]={{0.5f,-0.5f,16.f,true},1.f,0.f};
        tri.vertices[2]={{0.f,0.5f,16.f,true},1.f,0.f};
        eye.push_back(tri);
    }
    std::array<std::uint8_t,16> stripe{
        255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255
    };
    StereoWorldRaster::TextureRGBA four{stripe.data(),4,1,16,128};
    CHECK(raster.DrawTextured(manual,four));
    CHECK(Pixel(raster,0,512,512)==0xFF0000);
    CHECK(Pixel(raster,1,512,512)==0xFF0000);

    // D3D9 MODULATE(TEXTURE,DIFFUSE) must use perspective-correct
    // interpolation and filter alpha BEFORE the cutoff. SELECTARG1 stays
    // backwards-compatible even with non-white source attributes.
    raster.Clear();
    auto modulated=manual;
    for(auto& eye:modulated.eyes){
        eye[0].vertices[0].diffuse_rgba={{1.f,0.f,0.f,1.f}};
        eye[0].vertices[1].diffuse_rgba={{0.f,1.f,0.f,1.f}};
        eye[0].vertices[2].diffuse_rgba={{0.f,1.f,0.f,1.f}};
    }
    const std::array<std::uint8_t,4> white{{255,255,255,255}};
    StereoWorldRaster::TextureRGBA diffuse_image{
        white.data(),1,1,4,128,{}, {},true,false};
    CHECK(raster.DrawTextured(modulated,diffuse_image));
    for(int eye=0;eye<2;++eye){
        const auto pixel=Pixel(raster,eye,512,512);
        CHECK(((pixel>>16u)&255u)>190u);
        CHECK(((pixel>>8u)&255u)<65u);
        CHECK((pixel&255u)==0u);
    }
    raster.Clear();
    diffuse_image.modulate_rgb=false;
    CHECK(raster.DrawTextured(modulated,diffuse_image));
    CHECK(Pixel(raster,0,512,512)==0xffffffu);
    CHECK(Pixel(raster,1,512,512)==0xffffffu);
    raster.Clear();
    for(auto& eye:modulated.eyes)
        for(auto& vertex:eye[0].vertices)
            vertex.diffuse_rgba[3]=0.f;
    diffuse_image.modulate_alpha=true;
    CHECK(raster.DrawTextured(modulated,diffuse_image));
    CHECK(raster.DrawnPixels()==0u);
    raster.Clear();
    diffuse_image.modulate_alpha=false;
    CHECK(raster.DrawTextured(modulated,diffuse_image));
    CHECK(Pixel(raster,0,512,512)==0xffffffu);
    raster.Clear();
    modulated.eyes[1][0].vertices[0].diffuse_rgba[0]=
        (std::numeric_limits<float>::quiet_NaN)();
    CHECK(!raster.DrawTextured(modulated,diffuse_image));
    CHECK(raster.DrawnPixels()==0u);

    // The full independently rendered eye path must honor the original
    // D3D9 bilinear WRAP seam, not just the standalone sampler helper.
    raster.Clear();
    auto seam=manual;
    for(auto& eye:seam.eyes)
        for(auto& tri:eye)
            for(auto& vert:tri.vertices){vert.u=0.f;vert.v=0.f;}
    const std::array<std::uint8_t,16> quad{{
        255,0,0,0, 0,255,0,0,
        0,0,255,0, 255,255,255,255
    }};
    StereoWorldRaster::TextureRGBA wrap_image{
        quad.data(),2,2,8,32,
        {TextureAddress::Wrap,TextureAddress::Wrap,TextureFilter::Linear}};
    CHECK(raster.DrawTextured(seam,wrap_image));
    CHECK(Pixel(raster,0,512,512)==0x808080);
    CHECK(Pixel(raster,1,512,512)==0x808080);
    raster.Clear();
    wrap_image.alpha_cutoff=128; // blended seam alpha is only ~64
    CHECK(raster.DrawTextured(seam,wrap_image));
    CHECK(Pixel(raster,0,512,512)==0x000000);
    CHECK(Pixel(raster,1,512,512)==0x000000);
    raster.Clear();
    wrap_image.alpha_cutoff=0;
    wrap_image.sampler={};
    CHECK(raster.DrawTextured(seam,wrap_image));
    CHECK(Pixel(raster,0,512,512)==0xFF0000);
    CHECK(Pixel(raster,1,512,512)==0xFF0000);
    raster.Clear();
    auto invalid_sampler=wrap_image;
    invalid_sampler.sampler.filter=static_cast<TextureFilter>(9);
    CHECK(!raster.DrawTextured(seam,invalid_sampler));
    CHECK(raster.DrawnPixels()==0u);

    // Validate the whole batch before either eye can write.
    auto invalid=manual;
    invalid.eyes[1][0].vertices[2].u=
        (std::numeric_limits<float>::quiet_NaN)();
    const auto before=Pixel(raster,0,512,512);
    const auto drawn=raster.DrawnPixels();
    CHECK(!raster.DrawTextured(invalid,four));
    CHECK(raster.DrawnPixels()==drawn);
    CHECK(Pixel(raster,0,512,512)==before);
    auto bad_texture=four;
    bad_texture.stride=17;
    CHECK(!raster.DrawTextured(manual,bad_texture));
    const float bad_uv[]={0.f,1.f, 1.f,1.f,
       (std::numeric_limits<float>::infinity)(),0.f};
    const auto old_frame=textured.game_frame;
    CHECK(!BuildTexturedStereoBatch(near_vertices.data(),bad_uv,3,indices,3,
                                   100,latch,cal,world,textured));
    CHECK(textured.game_frame==old_frame);
    world.verified_cpu_pass=RenderPass::Hud2D;
    CHECK(!BuildTexturedStereoBatch(near_vertices.data(),uv,3,indices,3,
                                   100,latch,cal,world,textured));
    return 0;
}
