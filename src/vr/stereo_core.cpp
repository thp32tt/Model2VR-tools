#include "stereo_core.h"
#include <algorithm>
#include <cmath>

namespace m2vr::vr {
namespace {
bool finite(float f) { return std::isfinite(f); }
bool finite(Vec3 v) { return finite(v.x) && finite(v.y) && finite(v.z); }
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
float qlength2(const Quaternion& q) { return q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w; }
Quaternion conjugate(Quaternion q) { return {-q.x,-q.y,-q.z,q.w}; }
Quaternion normalized(Quaternion q) {
    const float length2=qlength2(q);
    if(!finite(length2) || length2<1e-9f) return {};
    const float inv=1.0f/std::sqrt(length2);
    return {q.x*inv,q.y*inv,q.z*inv,q.w*inv};
}
Projection project(Vec3 p,float near_depth) {
    if(!finite(p) || p.z <= near_depth) return {};
    return {p.x/p.z,p.y/p.z,true};
}
}  // namespace

Vec3 rotate(const Quaternion& raw, Vec3 v) {
    const auto q=normalized(raw);
    // Rotation by unit quaternion: p + 2w(q_xyz × p) + 2(q_xyz × (q_xyz × p)).
    const Vec3 t{2.0f*(q.y*v.z-q.z*v.y),
                 2.0f*(q.z*v.x-q.x*v.z),
                 2.0f*(q.x*v.y-q.y*v.x)};
    const Vec3 cross{q.y*t.z-q.z*t.y,q.z*t.x-q.x*t.z,q.x*t.y-q.y*t.x};
    return v + t*q.w + cross;
}
bool is_finite(const HeadPose& p) {
    const auto& q=p.rotation;
    return finite(p.position_m) && finite(q.x) && finite(q.y) &&
           finite(q.z) && finite(q.w) && qlength2(q)>1e-9f;
}
std::string_view pass_name(PassClass kind) {
    switch(kind){
    case PassClass::World3D:return "World3D";
    case PassClass::Hud2D:return "Hud2D";
    case PassClass::Sky:return "Sky";
    case PassClass::Mirror:return "Mirror";
    default:return "Unknown";
    }
}
bool GameProfile::set_view_config(ViewConfig config) {
    if(!finite(config.ipd_m)||config.ipd_m<=0.0f||config.ipd_m>0.09f ||
       !finite(config.units_per_meter)||config.units_per_meter<=0.0f||
       !finite(config.min_depth)||config.min_depth<=0.0f) return false;
    config_=config;
    return true;
}
bool GameProfile::set_caller(std::uint32_t caller_rva,PassClass kind) {
    if(!caller_rva) return false;
    for(auto& c:callers_) if(c.caller_rva==caller_rva){c.kind=kind;return true;}
    callers_.push_back({caller_rva,kind});
    return true;
}
PassClass GameProfile::classify(std::uint32_t caller_rva) const {
    for(const auto& c:callers_) if(c.caller_rva==caller_rva)return c.kind;
    return PassClass::Unknown; // fail-closed: unknown geometry is always mono.
}
bool StereoCoordinator::begin_frame(std::uint64_t id,const HeadPose& pose,const GameProfile& profile){
    if(current_ && id<=current_->frame_id) return false;
    if(pose.valid && !is_finite(pose))return false;
    const auto& cfg=profile.view_config();
    if(!finite(cfg.ipd_m)||cfg.ipd_m<=0.0f||!finite(cfg.units_per_meter)||cfg.units_per_meter<=0.0f||
       !finite(cfg.min_depth)||cfg.min_depth<=0.0f)return false;
    current_=FrameSnapshot{id,pose,cfg};
    return true;
}
StereoPoints StereoCoordinator::transform(Vec3 game_point,PassClass kind) const {
    StereoPoints result{game_point,game_point,false};
    if(!current_ || !finite(game_point))return result;
    const auto& frame=*current_;
    const auto& cfg=frame.config;
    if(kind!=PassClass::World3D && !(kind==PassClass::Sky && cfg.sky_stereo))return result;
    // Missing tracking is not a reason to invent a head pose. Fixed-eye zero-head pose is valid.
    Quaternion rotation{};
    if(frame.pose.valid && cfg.allow_head_rotation)rotation=normalized(frame.pose.rotation);
    const auto inv=conjugate(rotation);
    Vec3 game_center=game_point;
    if(kind!=PassClass::Sky && frame.pose.valid && cfg.allow_head_position)
        game_center=game_center-frame.pose.position_m*cfg.units_per_meter;
    const float half=cfg.ipd_m*0.5f*cfg.units_per_meter;
    // Eye coordinates relative to head (OpenXR +X is right). Convert world camera-space
    // points into the head's local eye space. Do not mutate game-owned vertex memory.
    const Vec3 left_offset{-half,0,0},right_offset{half,0,0};
    result.left=rotate(inv,game_center)-left_offset;
    result.right=rotate(inv,game_center)-right_offset;
    result.stereo=true;
    return result;
}
std::array<Projection,2> StereoCoordinator::project_preview(Vec3 point,PassClass kind) const{
    auto p=transform(point,kind);
    const float near_depth=current_?current_->config.min_depth:0.01f;
    return {project(p.left,near_depth),project(p.right,near_depth)};
}
}  // namespace m2vr::vr
