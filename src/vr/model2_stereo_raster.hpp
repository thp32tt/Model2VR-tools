#pragma once
// Experimental independent-eye opaque flat-color raster target.
// This bridge is deliberately separate from the D3D9 probe and FFB.
// No automatic world/HUD inference: the caller must supply a verified
// world draw plus real game view-space vertices and calibrated axes.
#include "model2_eye_clip.hpp"
#include "model2_textured_stereo.hpp"
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
    };
    // Fully clipped and validated beforehand; no half-eye write on a bad
    // texture, stale frame, invalid UV, or unsupported mixed pass.
    // Nearest clamp sampler, binary alpha test (NOT alpha blending).
    bool DrawTextured(const ClippedTexturedStereoBatch& batch,
                      const TextureRGBA& texture) noexcept {
        if(!texture.rgba || !texture.width || !texture.height ||
           texture.width>1024 || texture.height>1024 ||
           texture.stride!=texture.width*4u ||
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
    std::uint64_t AcceptedTriangles()const noexcept{return triangles_;}
private:
    struct Vertex {
        double x=0,y=0,inv_z=0;
    };
    static double Edge(const Vertex& a,const Vertex& b,
                       double x,double y) noexcept {
        return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
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
                const double a=Edge(v[1],v[2],px,py)/signed_area;
                const double b=Edge(v[2],v[0],px,py)/signed_area;
                const double c=1.0-a-b;
                if(a< -1.e-9||b< -1.e-9||c< -1.e-9)continue;
                // Perspective-correct depth via interpolation of 1/z.
                const double inverse=a*v[0].inv_z+b*v[1].inv_z+c*v[2].inv_z;
                if(!(inverse>0.)||!std::isfinite(inverse))continue;
                const double z=1./inverse;
                const auto offset=std::size_t(y)*Width+std::size_t(x);
                if(!(z<double(depth_[eye][offset])))continue;
                depth_[eye][offset]=static_cast<float>(z);
                const auto byte=offset*4u;
                color_[eye][byte]=static_cast<std::uint8_t>((rgb>>16)&255u);
                color_[eye][byte+1]=static_cast<std::uint8_t>((rgb>>8)&255u);
                color_[eye][byte+2]=static_cast<std::uint8_t>(rgb&255u);
                color_[eye][byte+3]=255;
                ++pixels_;
            }
        }
    }
    void TexturedTriangle(std::size_t eye,
                          const ClippedTexturedEyeTriangle& tri,
                          const TextureRGBA& texture) noexcept {
        std::array<Vertex,3> v{};
        std::array<double,3> u_over_z{},v_over_z{};
        for(std::size_t k=0;k<3;++k){
            const auto& p=tri.vertices[k].position;
            v[k]={ (double(p.ndc_x)*0.5+0.5)*Width,
                   (0.5-double(p.ndc_y)*0.5)*Height,
                   1.0/double(p.depth_m)};
            u_over_z[k]=double(tri.vertices[k].u)*v[k].inv_z;
            v_over_z[k]=double(tri.vertices[k].v)*v[k].inv_z;
        }
        const double signed_area=Edge(v[0],v[1],v[2].x,v[2].y);
        if(!std::isfinite(signed_area)||std::abs(signed_area)<1.e-8)
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
                const double a=Edge(v[1],v[2],px,py)/signed_area;
                const double b=Edge(v[2],v[0],px,py)/signed_area;
                const double c=1.0-a-b;
                if(a< -1.e-9||b< -1.e-9||c< -1.e-9)continue;
                const double inv=a*v[0].inv_z+b*v[1].inv_z+c*v[2].inv_z;
                if(!(inv>0.)||!std::isfinite(inv))continue;
                const double z=1./inv;
                const auto offset=std::size_t(y)*Width+std::size_t(x);
                if(!(z<double(depth_[eye][offset])))continue;
                const double u=(a*u_over_z[0]+b*u_over_z[1]+
                                c*u_over_z[2])/inv;
                const double tex_v=(a*v_over_z[0]+b*v_over_z[1]+
                                    c*v_over_z[2])/inv;
                if(!std::isfinite(u)||!std::isfinite(tex_v))continue;
                const double clamped_u=(std::clamp)(u,0.0,1.0);
                const double clamped_v=(std::clamp)(tex_v,0.0,1.0);
                const auto sx=(std::min)(texture.width-1u,
                     static_cast<std::uint32_t>(clamped_u*texture.width));
                const auto sy=(std::min)(texture.height-1u,
                     static_cast<std::uint32_t>(clamped_v*texture.height));
                const auto tex_index=
                     std::size_t(sy)*texture.stride+std::size_t(sx)*4u;
                const auto* texel=texture.rgba+tex_index;
                // Transparent texture cutouts must not occlude geometry.
                if(texel[3]<texture.alpha_cutoff)continue;
                const auto pixel=offset*4u;
                depth_[eye][offset]=static_cast<float>(z);
                auto& color=color_[eye];
                color[pixel]=texel[0];
                color[pixel+1]=texel[1];
                color[pixel+2]=texel[2];
                color[pixel+3]=255u;
                ++pixels_;
            }
        }
    }
    std::array<std::vector<std::uint8_t>,2> color_;
    std::array<std::vector<float>,2> depth_;
    std::uint64_t triangles_=0,pixels_=0;
};
} // namespace m2vr::vr
