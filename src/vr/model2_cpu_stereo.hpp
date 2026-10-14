#pragma once
// Model2 CPU view-space -> OpenXR per-eye projection. Original implementation.
// No game code executes here: it operates on immutable CPU vertices already
// transformed by the emulator. No global model, texture, HUD or FFB mutation.
#include "../common/stereo_pose_ipc.h"
#include "model2_render_policy.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace m2vr::vr {
struct Model2CameraCalibration {
    // Explicit row-major Model2-view -> XR-view axis conversion.
    // Must be determined by real Daytona geometry evidence.
    std::array<float,9> view_to_xr{};
    float units_per_meter=0.f;  // 0 means uncalibrated; never assume 1.
    float min_depth_m=0.05f;
    bool runtime_confirmed=false;
};
struct CpuPoint {float x=0,y=0,z=0;};
struct EyeProjection {float ndc_x=0,ndc_y=0,depth_m=0;bool valid=false;};
struct ProjectedPair {std::array<EyeProjection,2> eyes{};bool stereo=false;};
inline bool Finite(float x) noexcept{return std::isfinite(x);}
inline bool ValidCalibration(const Model2CameraCalibration& c) noexcept{
    if(!c.runtime_confirmed || !Finite(c.units_per_meter) || c.units_per_meter<=0.f ||
       !Finite(c.min_depth_m) || c.min_depth_m<=0.f) return false;
    for(float f:c.view_to_xr)if(!Finite(f))return false;
    // Orthogonal (possibly reflected) axis conversion only: no silent shear.
    for(int row=0;row<3;++row){
        float n=0;
        for(int k=0;k<3;++k)n+=c.view_to_xr[row*3+k]*c.view_to_xr[row*3+k];
        if(std::abs(n-1.f)>0.01f)return false;
        for(int other=0;other<row;++other){
            float d=0;
            for(int k=0;k<3;++k)d+=c.view_to_xr[row*3+k]*c.view_to_xr[other*3+k];
            if(std::abs(d)>0.01f)return false;
        }
    }
    return true;
}
inline CpuPoint RotateInverse(const float q[4],CpuPoint p) noexcept {
    // q is quaternion XYZW in OpenXR. Inverse rotation = conjugate(q).
    const float x=-q[0],y=-q[1],z=-q[2],w=q[3];
    const CpuPoint t{2.f*(y*p.z-z*p.y),2.f*(z*p.x-x*p.z),
                     2.f*(x*p.y-y*p.x)};
    return {p.x+w*t.x+(y*t.z-z*t.y),
            p.y+w*t.y+(z*t.x-x*t.z),
            p.z+w*t.z+(x*t.y-y*t.x)};
}
inline bool QuaternionValid(const float q[4]) noexcept{
    float n=0;
    for(int i=0;i<4;++i){if(!Finite(q[i]))return false;n+=q[i]*q[i];}
    return Finite(n) && n>=0.99f && n<=1.01f;
}
inline ProjectedPair ProjectModel2Stereo(
    CpuPoint already_transformed_game_point,
    const pose::Frame& latched_predicted_views,
    const Model2CameraCalibration& calibration,
    const DrawEvidence& pass) noexcept {
    ProjectedPair out{};
    if(ChooseEyeAction(pass)!=EyeAction::StereoGeometry ||
       !ValidCalibration(calibration) ||
       !Finite(already_transformed_game_point.x) ||
       !Finite(already_transformed_game_point.y) ||
       !Finite(already_transformed_game_point.z) ||
       !(latched_predicted_views.view_flags&pose::kOrientationValid) ||
       !(latched_predicted_views.view_flags&pose::kPositionValid) ||
       latched_predicted_views.sequence==0)return out;
    const auto& b=calibration.view_to_xr;
    const float u=calibration.units_per_meter;
    const CpuPoint world{
        (b[0]*already_transformed_game_point.x+b[1]*already_transformed_game_point.y+b[2]*already_transformed_game_point.z)/u,
        (b[3]*already_transformed_game_point.x+b[4]*already_transformed_game_point.y+b[5]*already_transformed_game_point.z)/u,
        (b[6]*already_transformed_game_point.x+b[7]*already_transformed_game_point.y+b[8]*already_transformed_game_point.z)/u
    };
    for(int eye=0;eye<2;++eye){
        const auto& view=latched_predicted_views.eyes[eye];
        if(!QuaternionValid(view.orientation_xyzw))return {};
        const CpuPoint relative{
            world.x-view.position_m[0],
            world.y-view.position_m[1],
            world.z-view.position_m[2]
        };
        const CpuPoint p=RotateInverse(view.orientation_xyzw,relative);
        const float depth=-p.z;
        if(!Finite(depth)||depth<=calibration.min_depth_m)return {};
        const auto& f=view.fov_left_right_up_down;
        const float l=std::tan(f[0]),r=std::tan(f[1]),
                    up=std::tan(f[2]),down=std::tan(f[3]);
        if(!Finite(l)||!Finite(r)||!Finite(up)||!Finite(down) ||
            !(r-l>0.01f)||!(up-down>0.01f))return {};
        const float sx=p.x/depth,sy=p.y/depth;
        const float nx=2.f*(sx-l)/(r-l)-1.f,ny=2.f*(sy-down)/(up-down)-1.f;
        if(!Finite(nx)||!Finite(ny))return {};
        out.eyes[eye]={nx,ny,depth,true};
    }
    out.stereo=true;
    return out;
}
} // namespace m2vr::vr
