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
        // Sutherland-Hodgman clipping cyclically rotates the polygon
        // vertex order even when all three source vertices are inside.
        // Check the original vertex survives in each eye, never presume
        // it occupies the first triangle corner.
        bool found_original=false;
        for(const auto& tri:eye)for(const auto& vertex:tri.vertices){
            const auto& rgba=vertex.diffuse_rgba;
            for(float channel:rgba)
                CHECK(std::isfinite(channel)&&channel>=0.f&&channel<=1.f);
            if(std::abs(rgba[0]-64.f/255.f)<0.0001f &&
               std::abs(rgba[1]-32.f/255.f)<0.0001f &&
               std::abs(rgba[2]-16.f/255.f)<0.0001f &&
               std::abs(rgba[3]-128.f/255.f)<0.0001f)
                found_original=true;
        }
        CHECK(found_original);
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

    // D3D9-style top-left ownership prevents a double-alpha seam where
    // two source triangles share a diagonal through pixel centers.
    raster.Clear();
    ClippedTexturedStereoBatch adjacent{};
    adjacent.game_frame=100;adjacent.pose_sequence=27;
    const auto make_vertex=[](float nx,float ny){
        TexturedEyeVertex point{};
        point.position={nx,ny,2.f,true};
        return point;
    };
    for(auto& eye:adjacent.eyes){
        ClippedTexturedEyeTriangle upper{},lower{};
        upper.vertices={{
            make_vertex(-0.5f,0.5f),
            make_vertex(0.5f,0.5f),
            make_vertex(0.5f,-0.5f)}};
        lower.vertices={{
            make_vertex(-0.5f,0.5f),
            make_vertex(0.5f,-0.5f),
            make_vertex(-0.5f,-0.5f)}};
        eye.push_back(upper);eye.push_back(lower);
    }
    const std::array<std::uint8_t,4> half_red_edge{255u,0u,0u,128u};
    StereoWorldRaster::TextureRGBA seam_material{
        half_red_edge.data(),1u,1u,4u,0u};
    seam_material.blend=BlendMode::SourceAlphaOver;
    seam_material.raster.write_depth=false;
    CHECK(raster.DrawTextured(adjacent,seam_material));
    for(int eye=0;eye<2;++eye){
        CHECK(Pixel(raster,eye,512,512)==0x800000u);
        CHECK(Pixel(raster,eye,520,500)==0x800000u);
        CHECK(Pixel(raster,eye,500,520)==0x800000u);
    }
    raster.Clear();
    for(auto& eye:adjacent.eyes)
        for(auto& tri:eye)std::swap(tri.vertices[1],tri.vertices[2]);
    CHECK(raster.DrawTextured(adjacent,seam_material));
    for(int eye=0;eye<2;++eye)
        CHECK(Pixel(raster,eye,512,512)==0x800000u);
    raster.Clear();
    // Original D3D9 FVF without a D3DCOLOR field provides opaque white
    // DIFFUSE/CURRENT. The x86 source adapter must never reject an otherwise
    // valid real-world material solely because the vertex has no color.
    raster.Clear();
    const std::array<std::uint8_t,4> unused_texel{12u,34u,56u,20u};
    StereoWorldRaster::TextureRGBA default_diffuse{
        unused_texel.data(),1u,1u,4u,255u,{}, {},false,false,
        BlendMode::Opaque,true,true};
    CHECK(raster.DrawTextured(manual,default_diffuse));
    for(int eye=0;eye<2;++eye){
        CHECK(Pixel(raster,eye,512,512)==0xffffffu);
        const auto& eye_rgba=raster.Eye(eye);
        const auto pixel=(std::size_t(512)*StereoWorldRaster::Width+512u)*4u;
        CHECK(eye_rgba[pixel+3u]==255u);
    }
    // Real TEX0 + D3DCOLOR supplies no UV bytes. The original x86
    // adapter synthesizes zero UV only after proof that both stage0
    // output channels select DIFFUSE/CURRENT. Exercise original packed
    // D3DCOLOR -> clipped independently shifted eyes -> real 8-bit RGBA.
    raster.Clear();
    const std::uint32_t tex0_argb[]{0x604080c0u,0x604080c0u,0x604080c0u};
    const float tex0_absent_uv[]{0.f,0.f,0.f,0.f,0.f,0.f};
    ClippedTexturedStereoBatch tex0_world{};
    CHECK(BuildTexturedStereoBatch(near_vertices.data(),
        tex0_absent_uv,3,indices,3,100,latch,cal,world,
        tex0_world,tex0_argb));
    CHECK(!tex0_world.eyes[0].empty() &&
          !tex0_world.eyes[1].empty());
    CHECK(tex0_world.eyes[0][0].vertices[0].position.ndc_x !=
          tex0_world.eyes[1][0].vertices[0].position.ndc_x);
    StereoWorldRaster::TextureRGBA tex0_diffuse{
        unused_texel.data(),1u,1u,4u,1u,{}, {},false,false,
        BlendMode::ReplaceRGBA,true,true};
    CHECK(raster.DrawTextured(tex0_world,tex0_diffuse));
    for(int eye=0;eye<2;++eye){
        CHECK(Pixel(raster,eye,512,512)==0x4080c0u);
        const auto& rgba=raster.Eye(eye);
        const auto pixel=(std::size_t(512)*StereoWorldRaster::Width+512u)*4u;
        CHECK(rgba[pixel+3u]==0x60u);
    }

    // MODULATE with missing vertex color is identically the source texel,
    // not a rejected or silently blackened original D3D9 primitive.
    raster.Clear();
    default_diffuse.alpha_cutoff=20u;
    default_diffuse.select_diffuse_rgb=false;
    default_diffuse.select_diffuse_alpha=false;
    default_diffuse.modulate_rgb=true;
    default_diffuse.modulate_alpha=true;
    CHECK(raster.DrawTextured(manual,default_diffuse));
    CHECK(Pixel(raster,0,512,512)==0x0c2238u);
    CHECK(Pixel(raster,1,512,512)==0x0c2238u);

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

    // Original D3DTOP_SELECTARG1(DIFFUSE) independently selects source
    // vertex RGB or alpha. It does NOT multiply the texture, and both
    // channels must use perspective-correct eye interpolation.
    raster.Clear();
    modulated.eyes[1][0].vertices[0].diffuse_rgba[0]=1.f;
    const std::array<std::uint8_t,4> pure_green{{0,255,0,255}};
    StereoWorldRaster::TextureRGBA selected{
        pure_green.data(),1,1,4,0,{}, {},false,false,
        BlendMode::Opaque,true,false};
    CHECK(raster.DrawTextured(modulated,selected));
    for(int eye=0;eye<2;++eye) {
        const auto rgb=Pixel(raster,eye,512,512);
        CHECK(((rgb>>16u)&255u)>190u);
        CHECK(((rgb>>8u)&255u)<65u);
        CHECK((rgb&255u)==0u);
    }
    // Alpha source may independently select diffuse while color stays
    // the sampled texture; zero diffuse alpha fails original alpha test.
    raster.Clear();
    selected.select_diffuse_rgb=false;
    selected.select_diffuse_alpha=true;
    selected.alpha_cutoff=1u;
    CHECK(raster.DrawTextured(modulated,selected));
    CHECK(raster.DrawnPixels()==0u);
    raster.Clear();
    for(auto& eye:modulated.eyes)
        for(auto& vertex:eye[0].vertices)
            vertex.diffuse_rgba[3]=0.5f;
    selected.alpha_cutoff=128u;
    CHECK(raster.DrawTextured(modulated,selected));
    CHECK(Pixel(raster,0,512,512)==0x00ff00u);
    CHECK(Pixel(raster,1,512,512)==0x00ff00u);
    raster.Clear();
    selected.alpha_cutoff=129u;
    CHECK(raster.DrawTextured(modulated,selected));
    CHECK(raster.DrawnPixels()==0u);
    // Unknown mixed same-channel select/multiply is rejected atomically.
    raster.Clear();
    selected.modulate_alpha=true;
    const auto before_conflict=raster.DrawnPixels();
    CHECK(!raster.DrawTextured(modulated,selected));
    CHECK(raster.DrawnPixels()==before_conflict);
    selected.modulate_alpha=false;

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

    // The x86 D3D9 MIRROR sampler reaches the same independent-eye
    // color/alpha renderer; both eyes must use a reflected edge texel.
    raster.Clear();
    auto mirror_geometry=seam;
    for(auto& eye:mirror_geometry.eyes)
        for(auto& tri:eye)for(auto& vert:tri.vertices){
            vert.u=1.25f;vert.v=0.25f;
        }
    auto mirror_texture=wrap_image;
    mirror_texture.sampler={
        TextureAddress::Mirror,TextureAddress::Mirror,TextureFilter::Point};
    CHECK(raster.DrawTextured(mirror_geometry,mirror_texture));
    CHECK(Pixel(raster,0,512,512)==0x00ff00);
    CHECK(Pixel(raster,1,512,512)==0x00ff00);

    // D3D9 non-separate alpha, additive, and premultiplied blending render
    // into two independent RGBA/Z images; no 2D-mirror-only shortcut.
    raster.Clear();
    const std::array<std::uint8_t,4> opaque_blue{{0,0,255,255}};
    const std::array<std::uint8_t,4> half_red{{255,0,0,128}};
    StereoWorldRaster::TextureRGBA layer{
        opaque_blue.data(),1,1,4,0};
    CHECK(raster.DrawTextured(manual,layer));
    layer.rgba=half_red.data();
    layer.raster.depth=DepthCompare::LessEqual;
    layer.blend=BlendMode::SourceAlphaOver;
    CHECK(raster.DrawTextured(manual,layer));
    for(int eye=0;eye<2;++eye)
        CHECK(Pixel(raster,eye,512,512)==0x80007fu);
    raster.Clear();
    layer.rgba=opaque_blue.data();layer.blend=BlendMode::Opaque;
    CHECK(raster.DrawTextured(manual,layer));
    layer.rgba=half_red.data();layer.blend=BlendMode::SourceAlphaAdd;
    CHECK(raster.DrawTextured(manual,layer));
    for(int eye=0;eye<2;++eye)
        CHECK(Pixel(raster,eye,512,512)==0x8000ffu);
    raster.Clear();
    layer.rgba=opaque_blue.data();layer.blend=BlendMode::Opaque;
    CHECK(raster.DrawTextured(manual,layer));
    layer.rgba=half_red.data();layer.blend=BlendMode::Additive;
    CHECK(raster.DrawTextured(manual,layer));
    for(int eye=0;eye<2;++eye)
        CHECK(Pixel(raster,eye,512,512)==0xff00ffu);
    raster.Clear();
    const std::array<std::uint8_t,4> premultiplied_red{{64,0,0,128}};
    layer.rgba=opaque_blue.data();layer.blend=BlendMode::Opaque;
    CHECK(raster.DrawTextured(manual,layer));
    layer.rgba=premultiplied_red.data();
    layer.blend=BlendMode::PremultipliedOver;
    CHECK(raster.DrawTextured(manual,layer));
    for(int eye=0;eye<2;++eye)
        CHECK(Pixel(raster,eye,512,512)==0x40007fu);
    // Original D3D9 blending DISABLED is still a full RGBA destination
    // write, not an implicit opaque source alpha. This matters when a
    // subsequent independent-eye material blends with DESTALPHA later.
    raster.Clear();
    layer.rgba=half_red.data();
    layer.blend=BlendMode::Opaque;
    CHECK(raster.DrawTextured(manual,layer));
    for(int eye=0;eye<2;++eye){
        const auto& rgba=raster.Eye(eye);
        const std::size_t offset=(std::size_t(512)*StereoWorldRaster::Width+
                                  512u)*4u;
        CHECK(rgba[offset]==255u && rgba[offset+1u]==0u &&
              rgba[offset+2u]==0u && rgba[offset+3u]==128u);
    }
    // Real ONE/ZERO does not force alpha=255. Later alpha blends see the
    // stored original D3D9 source alpha, not the legacy opaque sentinel.
    raster.Clear();
    layer.rgba=half_red.data();layer.blend=BlendMode::ReplaceRGBA;
    CHECK(raster.DrawTextured(manual,layer));
    for(int eye=0;eye<2;++eye){
        CHECK(Pixel(raster,eye,512,512)==0xff0000u);
        const auto& pixels=raster.Eye(eye);
        const auto center=(std::size_t(512)*StereoWorldRaster::Width+
                           std::size_t(512))*4u;
        CHECK(pixels[center+3u]==128u);
    }
    // Preserve last composed sample under an invalid material state.
    const auto blended=Pixel(raster,0,512,512);
    const auto count_before_invalid=raster.DrawnPixels();
    layer.blend=static_cast<BlendMode>(99u);
    CHECK(!raster.DrawTextured(manual,layer));
    CHECK(raster.DrawnPixels()==count_before_invalid);
    CHECK(Pixel(raster,0,512,512)==blended);
    // Original x86 D3DTADDRESS_MIRRORONCE traverses the real
    // independent-eye texture path, not only the standalone sampler.
    raster.Clear();
    auto once_texture=mirror_texture;
    once_texture.sampler={
        TextureAddress::MirrorOnce,TextureAddress::MirrorOnce,
        TextureFilter::Point};
    for(auto& eye:mirror_geometry.eyes)
        for(auto& tri:eye)
            for(auto& v:tri.vertices){v.u=-1.75f;v.v=0.25f;}
    CHECK(raster.DrawTextured(mirror_geometry,once_texture));
    CHECK(Pixel(raster,0,512,512)==0x00ff00u);
    CHECK(Pixel(raster,1,512,512)==0x00ff00u);
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
