#pragma once
// Source-independent OpenXR original-game 2D display placement.
// Captures one static world-space quad in front of the actual HMD
// *at first source-frame reception*, instead of assuming the OpenXR
// LOCAL-space origin coincides with the current seated driver position.
// No renderer or emulator access and no claim of a calibrated game camera.
#include <cmath>
namespace m2vr::monoquad {
struct Vec3 { float x=0.f,y=0.f,z=0.f; };
struct Quat { float x=0.f,y=0.f,z=0.f,w=1.f; };
struct View { Vec3 position{}; Quat orientation{}; };
struct Placement {
    Vec3 position{};
    Quat orientation{};
    float width=2.0f,height=2.0f;
};
inline bool Finite(Vec3 p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
inline bool Finite(Quat q) noexcept {
    return std::isfinite(q.x)&&std::isfinite(q.y)&&
           std::isfinite(q.z)&&std::isfinite(q.w);
}
inline float Dot(Quat a,Quat b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
}
inline bool Normalize(Quat& q) noexcept {
    if(!Finite(q))return false;
    const float len2=Dot(q,q);
    // Reject corrupt tracking while allowing common float precision.
    if(!std::isfinite(len2) || len2<0.5f || len2>1.5f)return false;
    const float inverse=1.0f/std::sqrt(len2);
    q.x*=inverse;q.y*=inverse;q.z*=inverse;q.w*=inverse;
    return true;
}
inline Vec3 Rotate(Quat q,Vec3 v) noexcept {
    const Vec3 t{2.f*(q.y*v.z-q.z*v.y),
                 2.f*(q.z*v.x-q.x*v.z),
                 2.f*(q.x*v.y-q.y*v.x)};
    return {v.x+q.w*t.x+q.y*t.z-q.z*t.y,
            v.y+q.w*t.y+q.z*t.x-q.x*t.z,
            v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
inline bool CapturePlacement(const View& left,const View& right,
                             Placement& output,float distance_m=1.65f,
                             float width_m=2.0f) noexcept {
    if(!std::isfinite(distance_m)||distance_m<0.5f||distance_m>4.0f ||
       !std::isfinite(width_m)||width_m<0.5f||width_m>4.0f)return false;
    if(!Finite(left.position)||!Finite(right.position) ||
       !Finite(left.orientation)||!Finite(right.orientation))return false;
    const Vec3 delta{left.position.x-right.position.x,
                     left.position.y-right.position.y,
                     left.position.z-right.position.z};
    const float distance2=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z;
    if(!std::isfinite(distance2)||distance2>0.30f*0.30f)return false;
    Quat a=left.orientation,b=right.orientation;
    if(!Normalize(a)||!Normalize(b))return false;
    float dot=Dot(a,b);
    if(dot<0.f) {
        b.x=-b.x;b.y=-b.y;b.z=-b.z;b.w=-b.w;
        dot=-dot;
    }
    // Both eyes describe essentially one rigid HMD, not two unrelated
    // orientations. A corrupt pair cannot place a quad behind the user.
    if(dot<0.95f)return false;
    Quat middle{a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w};
    if(!Normalize(middle))return false;
    const Vec3 center{(left.position.x+right.position.x)*0.5f,
                      (left.position.y+right.position.y)*0.5f,
                      (left.position.z+right.position.z)*0.5f};
    // Keep original 2D Daytona upright and level with the driver's eyes.
    // A full HMD pitch/roll would anchor a flat cinema screen into the
    // floor or ceiling if the player happened to glance down at startup.
    // Use actual HMD yaw, preserving real head CENTER as screen origin.
    const Vec3 forward=Rotate(middle,{0.f,0.f,-1.f});
    float fx=forward.x,fz=forward.z;
    float horizontal=std::sqrt(fx*fx+fz*fz);
    if(!std::isfinite(horizontal))return false;
    if(horizontal<0.15f){
        // Near-vertical HMD gaze: recover the yaw from projected local
        // right axis; this avoids 180-degree singularities.
        const Vec3 rightward=Rotate(middle,{1.f,0.f,0.f});
        fx=rightward.z;fz=-rightward.x;
        horizontal=std::sqrt(fx*fx+fz*fz);
        if(!std::isfinite(horizontal)||horizontal<0.15f)return false;
    }
    fx/=horizontal;fz/=horizontal;
    const float yaw=std::atan2(-fx,-fz);
    const float half_yaw=yaw*0.5f;
    Placement next{};
    next.position={center.x+fx*distance_m,center.y,
                   center.z+fz*distance_m};
    next.orientation={0.f,std::sin(half_yaw),0.f,std::cos(half_yaw)};
    next.width=width_m;
    next.height=width_m;
    if(!Finite(next.position))return false;
    output=next;
    return true;
}
class WorldFixedAnchor {
public:
    bool CaptureOnce(const View& left,const View& right,
                     float distance_m=1.65f,float width_m=2.f) noexcept {
        if(valid_)return true; // head translation/rotation does NOT chase screen
        Placement p{};
        if(!CapturePlacement(left,right,p,distance_m,width_m))return false;
        placement_=p;
        valid_=true;
        return true;
    }
    bool Valid() const noexcept {return valid_;}
    const Placement& Get() const noexcept {return placement_;}
    void Reset() noexcept {valid_=false;placement_={};}
private:
    Placement placement_{};
    bool valid_=false;
};
} // namespace m2vr::monoquad
