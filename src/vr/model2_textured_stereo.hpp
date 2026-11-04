#pragma once
// Verified-view-space textured world triangles, one game-frame pose latch.
// UV attributes are clipped in homogeneous coordinates BEFORE perspective
// division and sampled with perspective correction in the independent eyes.
// Never modifies D3D9 state, game memory, simulation or FFB.
#include "model2_eye_clip.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace m2vr::vr {
struct TexturedEyeVertex {
    EyeProjection position{};
    float u=0.f,v=0.f;
    // D3D9 diffuse vertex channels, linear in unprojected camera space.
    // White is the backwards-compatible SELECTARG1(TEXTURE) default.
    std::array<float,4> diffuse_rgba{{1.f,1.f,1.f,1.f}};
};
struct ClippedTexturedEyeTriangle {
    std::array<TexturedEyeVertex,3> vertices{};
};
struct ClippedTexturedStereoBatch {
    std::array<std::vector<ClippedTexturedEyeTriangle>,2> eyes{};
    std::uint64_t game_frame=0,pose_sequence=0;
};

namespace texclip_detail {
struct Attributed {
    clip_detail::Vertex clip{};
    double u=0,v=0;
    std::array<double,4> diffuse{{1.,1.,1.,1.}};
};
inline bool Finite(const Attributed& a) noexcept {
    return clip_detail::Finite3(a.clip) &&
           std::isfinite(a.u) && std::isfinite(a.v) &&
           std::abs(a.u)<=65536. && std::abs(a.v)<=65536. &&
           std::all_of(a.diffuse.begin(),a.diffuse.end(),[](double c){
               return std::isfinite(c) && c>=-1.e-6 && c<=1.000001;
           });
}
inline bool Intersect(const Attributed& a,const Attributed& b,
                      double da,double db,Attributed& result) noexcept {
    if(!clip_detail::Intersect(a.clip,b.clip,da,db,result.clip))return false;
    const double denom=da-db;
    const double t=da/denom;
    result.u=a.u+t*(b.u-a.u);
    result.v=a.v+t*(b.v-a.v);
    for(std::size_t i=0;i<4;++i)
        result.diffuse[i]=a.diffuse[i]+t*(b.diffuse[i]-a.diffuse[i]);
    return Finite(result);
}
inline bool Clip(const std::array<Attributed,3>& input,double near_z,
                 std::array<Attributed,12>& poly,std::size_t& count) noexcept {
    poly={};
    for(std::size_t i=0;i<3;++i)poly[i]=input[i];
    count=3;
    std::array<Attributed,12> next{};
    for(int plane=0;plane<5;++plane){
        if(!count)return true;
        std::size_t used=0;
        for(std::size_t i=0;i<count;++i){
            const auto& a=poly[i];
            const auto& b=poly[(i+1)%count];
            const double da=clip_detail::Distance(a.clip,plane,near_z);
            const double db=clip_detail::Distance(b.clip,plane,near_z);
            if(!std::isfinite(da)||!std::isfinite(db))return false;
            const bool ina=da>=0.,inb=db>=0.;
            if(ina!=inb){
                Attributed intersection{};
                if(used>=next.size() ||
                   !Intersect(a,b,da,db,intersection))return false;
                next[used++]=intersection;
            }
            if(inb){
                if(used>=next.size())return false;
                next[used++]=b;
            }
        }
        poly=next;
        count=used;
    }
    return true;
}
inline bool Normalize(const Attributed& vertex,double near_z,
                      TexturedEyeVertex& output) noexcept {
    if(!Finite(vertex) ||
       !clip_detail::Normalize(vertex.clip,near_z,output.position))return false;
    output.u=static_cast<float>(vertex.u);
    output.v=static_cast<float>(vertex.v);
    for(std::size_t i=0;i<4;++i)
        output.diffuse_rgba[i]=static_cast<float>(std::clamp(
            vertex.diffuse[i],0.,1.));
    return std::isfinite(output.u)&&std::isfinite(output.v);
}
}

inline bool BuildTexturedStereoBatch(
    const CpuPoint* positions,const float* uv_interleaved,
    std::size_t vertex_count,const std::uint32_t* indices,
    std::size_t index_count,std::uint64_t game_frame,
    const Model2FramePoseLatch& latch,
    const Model2CameraCalibration& cal,
    const DrawEvidence& evidence,
    ClippedTexturedStereoBatch& output,
    const std::uint32_t* diffuse_argb=nullptr) {
    if(!positions||!uv_interleaved||!indices||!vertex_count||
       vertex_count>8192 || !index_count||index_count%3||
       index_count/3>8192 ||
       ChooseEyeAction(evidence)!=EyeAction::StereoGeometry ||
       !ValidCalibration(cal))return false;
    pose::Frame frame{};
    if(!latch.Read(game_frame,frame))return false;
    ClippedTexturedStereoBatch next{};
    next.game_frame=game_frame;
    next.pose_sequence=frame.sequence;
    for(int eye_index=0;eye_index<2;++eye_index){
        const auto& eye=frame.eyes[eye_index];
        auto& output_triangles=next.eyes[eye_index];
        for(std::size_t i=0;i<index_count;i+=3){
            std::array<texclip_detail::Attributed,3> original{};
            for(std::size_t corner=0;corner<3;++corner){
                const auto index=indices[i+corner];
                if(index>=vertex_count)return false;
                auto& v=original[corner];
                if(!clip_detail::ToEyeClip(positions[index],eye,cal,v.clip))
                    return false;
                v.u=static_cast<double>(uv_interleaved[index*2]);
                v.v=static_cast<double>(uv_interleaved[index*2+1]);
                if(diffuse_argb){
                    const auto argb=diffuse_argb[index];
                    v.diffuse={{
                        double((argb>>16u)&255u)/255.,
                        double((argb>>8u)&255u)/255.,
                        double(argb&255u)/255.,
                        double((argb>>24u)&255u)/255.
                    }};
                }
                if(!texclip_detail::Finite(v))return false;
            }
            std::array<texclip_detail::Attributed,12> clipped{};
            std::size_t count=0;
            if(!texclip_detail::Clip(original,cal.min_depth_m,clipped,count))
                return false;
            for(std::size_t j=1;j+1<count;++j){
                ClippedTexturedEyeTriangle tri{};
                if(!texclip_detail::Normalize(clipped[0],cal.min_depth_m,tri.vertices[0]) ||
                   !texclip_detail::Normalize(clipped[j],cal.min_depth_m,tri.vertices[1]) ||
                   !texclip_detail::Normalize(clipped[j+1],cal.min_depth_m,tri.vertices[2]))
                    return false;
                const auto& a=tri.vertices[0].position;
                const auto& b=tri.vertices[1].position;
                const auto& c=tri.vertices[2].position;
                const double area=
                    (double(b.ndc_x)-a.ndc_x)*(double(c.ndc_y)-a.ndc_y)-
                    (double(b.ndc_y)-a.ndc_y)*(double(c.ndc_x)-a.ndc_x);
                if(std::abs(area)<1.e-12)continue;
                if(output_triangles.size()>=49152)return false;
                output_triangles.push_back(tri);
            }
        }
    }
    output=std::move(next);
    return true;
}
} // namespace m2vr::vr
