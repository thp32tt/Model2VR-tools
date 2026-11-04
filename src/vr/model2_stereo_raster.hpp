#pragma once
// Experimental independent-eye opaque flat-color raster target.
// This bridge is deliberately separate from the D3D9 probe and FFB.
// No automatic world/HUD inference: the caller must supply a verified
// world draw plus real game view-space vertices and calibrated axes.
#include "model2_eye_clip.hpp"
#include "model2_textured_stereo.hpp"
#include "model2_texture_sampler.hpp"
#include "model2_raster_states.hpp"
#include "model2_blend_states.hpp"
#include "../common/stereo_frame_ipc.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace m2vr::vr {
class StereoWorldRaster {
public:
    static constexpr std::uint32_t Width=transport::kWidth;
    static constexpr std::uint32_t Height=transport::kHeight;
    StereoWorldRaster() {
        for(auto& e:color_)e.resize(transport::kBytesPerEye);
        for(auto& z:depth_)z.resize(std::size_t(Width)*Height);
        Clear();
    }
    void Clear() noexcept {
        for(int eye=0;eye<2;++eye) {
            auto& c=color_[eye];
            for(std::size_t i=0;i<c.size();i+=4) {
                c[i]=0;c[i+1]=0;c[i+2]=0;c[i+3]=255;
            }
            std::fill(depth_[eye].begin(),depth_[eye].end(),
                      (std::numeric_limits<float>::infinity)());
        }
        triangles_=0;
        pixels_=0;
        eye_pixels_.fill(0u);
    }
    // Entire clip result is validated by BuildClippedStereoBatch before
    // any pixels are changed. Reversed triangle winding is accepted.
    bool Draw(const ClippedStereoBatch& batch,
              const std::uint32_t* rgb_colors,
              std::size_t source_triangle_count) noexcept {
        if(!rgb_colors || source_triangle_count==0 ||
           source_triangle_count>8192 ||
           batch.game_frame==0 || batch.pose_sequence==0)return false;
        for(const auto& eye:batch.eyes) {
            for(const auto& t:eye) {
                if(t.source_triangle>=source_triangle_count)return false;
                for(const auto& p:t.vertices) {
                    if(!p.valid||!std::isfinite(p.ndc_x)||
                       !std::isfinite(p.ndc_y)||!std::isfinite(p.depth_m)||
                       p.depth_m<=0.f||p.ndc_x< -1.f||p.ndc_x>1.f||
                       p.ndc_y< -1.f||p.ndc_y>1.f)return false;
                }
            }
        }
        for(std::size_t eye=0;eye<2;++eye) {
            for(const auto& tri:batch.eyes[eye])
                Triangle(eye,tri,rgb_colors[tri.source_triangle]);
        }
        return true;
    }
    struct TextureRGBA {
        const std::uint8_t* rgba=nullptr;
        std::uint32_t width=0,height=0,stride=0;
        std::uint8_t alpha_cutoff=128;
        SamplerPolicy sampler{};
        RasterPolicy raster{};
        bool modulate_rgb=false;
        bool modulate_alpha=false;
        BlendMode blend=BlendMode::Opaque;
        // New source-backed D3DTOP_SELECTARGx(DIFFUSE) channels.
        // Kept after legacy fields so existing aggregate ABI is intact.
        bool select_diffuse_rgb=false;
        bool select_diffuse_alpha=false;
    };
    // Fully clipped and validated beforehand; no half-eye write on a bad
    // texture, stale frame, invalid UV, or unsupported mixed pass.
    // Original alpha-test followed by exact non-separate source blending.
    bool DrawTextured(const ClippedTexturedStereoBatch& batch,
                      const TextureRGBA& texture) noexcept {
        const bool diffuse_only=texture.select_diffuse_rgb &&
                                texture.select_diffuse_alpha;
        // A textureless original TEX0 Draw is only valid if both fixed
        // stage-zero channels select DIFFUSE/CURRENT. Mixed channels must
        // never substitute a fictional texture or color.
        if((texture.rgba ?
            (!texture.width || !texture.height ||
             texture.width>1024 || texture.height>1024 ||
             texture.stride!=texture.width*4u) :
            (!diffuse_only || texture.width || texture.height ||
             texture.stride)) ||
           !ValidSampler(texture.sampler) ||
           !ValidRasterPolicy(texture.raster) ||
           !ValidBlendMode(texture.blend) ||
           (texture.select_diffuse_rgb && texture.modulate_rgb) ||
           (texture.select_diffuse_alpha && texture.modulate_alpha) ||
           batch.game_frame==0 || batch.pose_sequence==0)
            return false;
        for(const auto& eye:batch.eyes){
            for(const auto& tri:eye){
                for(const auto& v:tri.vertices){
                    const auto& p=v.position;
                    if(!p.valid || !std::isfinite(p.ndc_x) ||
                       !std::isfinite(p.ndc_y) ||
                       !std::isfinite(p.depth_m) || p.depth_m<=0.f ||
                       p.ndc_x< -1.f || p.ndc_x>1.f ||
                       p.ndc_y< -1.f || p.ndc_y>1.f ||
                       !std::isfinite(v.u) || !std::isfinite(v.v) ||
                       std::abs(v.u)>65536.f || std::abs(v.v)>65536.f)
                        return false;
                    for(const float channel:v.diffuse_rgba)
                        if(!std::isfinite(channel) || channel<0.f ||
                           channel>1.f)return false;
                }
            }
        }
        for(std::size_t eye=0;eye<2;++eye)
            for(const auto& tri:batch.eyes[eye])
                TexturedTriangle(eye,tri,texture);
        return true;
    }
    const std::vector<std::uint8_t>& Eye(std::size_t index)const noexcept {
        return color_[index];
    }
    std::uint64_t DrawnPixels()const noexcept{return pixels_;}
    std::uint64_t WrittenEyePixels(std::size_t eye)const noexcept {
        return eye<2u?eye_pixels_[eye]:0u;
    }
    // A fully clipped eye or two visually identical images is not a
    // verified native binocular source, even after valid Draw calls.
    bool StereoPairUsable()const noexcept {
        if(!eye_pixels_[0] || !eye_pixels_[1])return false;
        // OpenXR eye-color proof is RGB, not a difference solely in the
        // original destination alpha plane. A masked alpha-only Draw
        // cannot turn an otherwise identical black pair into true stereo.
        const auto& left=color_[0];
        const auto& right=color_[1];
        for(std::size_t i=0;i<left.size();i+=4u)
            if(left[i]!=right[i] || left[i+1u]!=right[i+1u] ||
               left[i+2u]!=right[i+2u])return true;
        return false;
    }
    std::uint64_t AcceptedTriangles()const noexcept{return triangles_;}
private:
    struct Vertex {
        double x=0,y=0,inv_z=0;
    };
    static double Edge(const Vertex& a,const Vertex& b,
                       double x,double y) noexcept {
        return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
    }
    // A source pixel on a shared edge belongs to exactly one triangle.
    // Normalize the winding for the D3D9-style top-left fill convention;
    // without ownership, translucent triangle strips blend twice on seams.
    static bool BarycentricTopLeft(const std::array<Vertex,3>& v,
                                   double signed_area,double px,double py,
                                   double& a,double& b,double& c) noexcept {
        const double sign=signed_area>0. ? 1. : -1.;
        const double wa=sign*Edge(v[1],v[2],px,py);
        const double wb=sign*Edge(v[2],v[0],px,py);
        const double wc=sign*Edge(v[0],v[1],px,py);
        const auto inside=[sign](double weight,const Vertex& start,
                                 const Vertex& end) noexcept {
            constexpr double epsilon=1.e-9;
            if(weight>epsilon)return true;
            if(weight< -epsilon)return false;
            const double dx=(end.x-start.x)*sign;
            const double dy=(end.y-start.y)*sign;
            return dy<0. || (dy==0. && dx>0.);
        };
        if(!inside(wa,v[1],v[2]) ||
           !inside(wb,v[2],v[0]) ||
           !inside(wc,v[0],v[1]))return false;
        const double denominator=std::abs(signed_area);
        a=wa/denominator;b=wb/denominator;c=wc/denominator;
        return true;
    }
    void Triangle(std::size_t eye,const ClippedEyeTriangle& tri,
                  std::uint32_t rgb) noexcept {
        std::array<Vertex,3> v{};
        for(std::size_t k=0;k<3;++k){
            const auto& p=tri.vertices[k];
            v[k]={ (double(p.ndc_x)*0.5+0.5)*Width,
                   (0.5-double(p.ndc_y)*0.5)*Height,
                   1.0/double(p.depth_m)};
        }
        const double signed_area=Edge(v[0],v[1],v[2].x,v[2].y);
        if(!std::isfinite(signed_area) ||
           std::abs(signed_area)<1.e-8)return;
        const double left=(std::max)(0.0,std::floor((std::min)({
            v[0].x,v[1].x,v[2].x})));
        const double right=(std::min)(double(Width)-1.,
            std::ceil((std::max)({v[0].x,v[1].x,v[2].x})));
        const double top=(std::max)(0.0,std::floor((std::min)({
            v[0].y,v[1].y,v[2].y})));
        const double bottom=(std::min)(double(Height)-1.,
            std::ceil((std::max)({v[0].y,v[1].y,v[2].y})));
        if(left>right || top>bottom)return;
        const auto x0=static_cast<int>(left),x1=static_cast<int>(right);
        const auto y0=static_cast<int>(top),y1=static_cast<int>(bottom);
        ++triangles_;
        for(int y=y0;y<=y1;++y){
            for(int x=x0;x<=x1;++x){
                const double px=double(x)+0.5,py=double(y)+0.5;
                double a=0.,b=0.,c=0.;
                if(!BarycentricTopLeft(v,signed_area,px,py,a,b,c))
                    continue;
                // Perspective-correct depth via interpolation of 1/z.
                const double inverse=a*v[0].inv_z+b*v[1].inv_z+c*v[2].inv_z;
                if(!(inverse>0.)||!std::isfinite(inverse))continue;
                const double z=1./inverse;
                const auto offset=std::size_t(y)*Width+std::size_t(x);
                // Quantize source Z before strict LESS as with textured passes.
                if(!DepthPasses(z,depth_[eye][offset],
                                DepthCompare::Less))continue;
                depth_[eye][offset]=static_cast<float>(z);
                const auto byte=offset*4u;
                color_[eye][byte]=static_cast<std::uint8_t>((rgb>>16)&255u);
                color_[eye][byte+1]=static_cast<std::uint8_t>((rgb>>8)&255u);
                color_[eye][byte+2]=static_cast<std::uint8_t>(rgb&255u);
                color_[eye][byte+3]=255;
                ++pixels_;
                ++eye_pixels_[eye];
            }
        }
    }
    void TexturedTriangle(std::size_t eye,
                          const ClippedTexturedEyeTriangle& tri,
                          const TextureRGBA& texture) noexcept {
        std::array<Vertex,3> v{};
        std::array<double,3> u_over_z{},v_over_z{};
        std::array<std::array<double,4>,3> diffuse_over_z{};
        for(std::size_t k=0;k<3;++k){
            const auto& p=tri.vertices[k].position;
            v[k]={ (double(p.ndc_x)*0.5+0.5)*Width,
                   (0.5-double(p.ndc_y)*0.5)*Height,
                   1.0/double(p.depth_m)};
            u_over_z[k]=double(tri.vertices[k].u)*v[k].inv_z;
            v_over_z[k]=double(tri.vertices[k].v)*v[k].inv_z;
            for(std::size_t c=0;c<4;++c)
                diffuse_over_z[k][c]=
                    double(tri.vertices[k].diffuse_rgba[c])*v[k].inv_z;
        }
        const double signed_area=Edge(v[0],v[1],v[2].x,v[2].y);
        if(!TriangleSurvivesCull(signed_area,texture.raster.cull))
            return;
        const int x0=static_cast<int>((std::max)(0.0,
            std::floor((std::min)({v[0].x,v[1].x,v[2].x}))));
        const int x1=static_cast<int>((std::min)(double(Width)-1.,
            std::ceil((std::max)({v[0].x,v[1].x,v[2].x}))));
        const int y0=static_cast<int>((std::max)(0.0,
            std::floor((std::min)({v[0].y,v[1].y,v[2].y}))));
        const int y1=static_cast<int>((std::min)(double(Height)-1.,
            std::ceil((std::max)({v[0].y,v[1].y,v[2].y}))));
        if(x0>x1||y0>y1)return;
        ++triangles_;
        for(int y=y0;y<=y1;++y){
            for(int x=x0;x<=x1;++x){
                const double px=double(x)+0.5,py=double(y)+0.5;
                double a=0.,b=0.,c=0.;
                if(!BarycentricTopLeft(v,signed_area,px,py,a,b,c))
                    continue;
                const double inv=a*v[0].inv_z+b*v[1].inv_z+c*v[2].inv_z;
                if(!(inv>0.)||!std::isfinite(inv))continue;
                const double z=1./inv;
                const auto offset=std::size_t(y)*Width+std::size_t(x);
                if(!DepthPasses(z,depth_[eye][offset],
                     texture.raster.depth))continue;
                SampledRGBA texel{255u,255u,255u,255u};
                if(!(texture.select_diffuse_rgb &&
                     texture.select_diffuse_alpha)){
                    const double u=(a*u_over_z[0]+b*u_over_z[1]+
                                    c*u_over_z[2])/inv;
                    const double tex_v=(a*v_over_z[0]+b*v_over_z[1]+
                                        c*v_over_z[2])/inv;
                    if(!std::isfinite(u)||!std::isfinite(tex_v))continue;
                    if(!SampleTextureRGBA(texture.rgba,texture.width,
                         texture.height,texture.stride,u,tex_v,
                         texture.sampler,texel))continue;
                }
                // Bilinear alpha is filtered BEFORE the alpha cutoff,
                // mirroring the fixed-function texture sample pipeline.
                // Source D3D9 COLOROP/ALPHAOP MODULATE(TEXTURE,DIFFUSE):
                // perspective-correct diffuse color, never NDC-linear.
                std::array<double,4> diffuse{{1.,1.,1.,1.}};
                bool diffuse_valid=true;
                if(texture.modulate_rgb || texture.modulate_alpha ||
                   texture.select_diffuse_rgb || texture.select_diffuse_alpha){
                    for(std::size_t channel=0;channel<4;++channel){
                        const double value=(a*diffuse_over_z[0][channel]+
                            b*diffuse_over_z[1][channel]+
                            c*diffuse_over_z[2][channel])/inv;
                        if(!std::isfinite(value)){
                            diffuse_valid=false;
                            break;
                        }
                        diffuse[channel]=std::clamp(value,0.,1.);
                    }
                }
                if(!diffuse_valid)continue; // reject before Z/color writes
                const auto sampled_alpha=static_cast<unsigned>(
                    std::lround(texture.select_diffuse_alpha ?
                        (255.*diffuse[3]) :
                        (double(texel.a)*
                         (texture.modulate_alpha?diffuse[3]:1.))));
                if(sampled_alpha<texture.alpha_cutoff)continue;
                const auto pixel=offset*4u;
                if(texture.raster.write_depth)
                    depth_[eye][offset]=static_cast<float>(z);
                auto& color=color_[eye];
                std::array<std::uint8_t,4> src_rgba{
                    texel.r,texel.g,texel.b,
                    static_cast<std::uint8_t>(sampled_alpha)};
                for(std::size_t channel=0;channel<3;++channel){
                    const double channel_value=texture.select_diffuse_rgb ?
                        (255.*diffuse[channel]) :
                        (double(src_rgba[channel])*
                         (texture.modulate_rgb?diffuse[channel]:1.));
                    src_rgba[channel]=static_cast<std::uint8_t>(
                        std::lround(std::clamp(channel_value,0.,255.)));
                }
                // Per-channel output mask applies after blending. Z-only passes
                // do not establish visible binocular color coverage.
                CompositeSourceRGBA(color.data()+pixel,src_rgba,
                    texture.blend,texture.raster.write_rgba_mask);
                if((texture.raster.write_rgba_mask&0x07u) &&
                   BlendProducesColor(texture.blend)) {
                    ++pixels_;
                    ++eye_pixels_[eye];
                }
            }
        }
    }
    std::array<std::vector<std::uint8_t>,2> color_;
    std::array<std::vector<float>,2> depth_;
    std::uint64_t triangles_=0,pixels_=0;
    std::array<std::uint64_t,2> eye_pixels_{};
};
} // namespace m2vr::vr
