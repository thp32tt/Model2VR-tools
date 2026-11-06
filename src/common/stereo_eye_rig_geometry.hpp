#pragma once
// OpenXR XR_PRIMARY_STEREO physical left/right rig shape shared by the
// Win64 pose publisher, Win32 original-device source gate and CPU-world
// recenter. This is NOT game camera calibration or a World3D classifier.
// Source eye positions/quaternions/FOV never leave the user's runtime.
#include "stereo_pose_ipc.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace m2vr::pose {
enum class StereoRigRejection : std::uint32_t {
    None=0u, MissingTrackedSource=1u, InvalidEye=2u,
    UnphysicalFov=3u, DivergentEyeRotation=4u,
    InvalidIpd=5u, ReversedEyeOrder=6u, MismatchedEyeFov=7u
};
struct StereoRigCheck {
    StereoRigRejection reason=StereoRigRejection::MissingTrackedSource;
    float ipd_m=0.f;
    constexpr bool valid() const noexcept {
        return reason==StereoRigRejection::None;
    }
};
inline StereoRigCheck CheckStereoRigGeometry(const Frame& source) noexcept {
    if(!source.sequence ||
       (source.view_flags&(kOrientationValid|kPositionValid))!=
         (kOrientationValid|kPositionValid))
        return {StereoRigRejection::MissingTrackedSource,0.f};
    double norms[2]{},horizontal[2]{},vertical[2]{};
    for(int eye_index=0;eye_index<2;++eye_index){
        const auto& eye=source.eyes[eye_index];
        double norm=0.;
        for(const float q:eye.orientation_xyzw){
            if(!std::isfinite(q))return {StereoRigRejection::InvalidEye,0.f};
            norm+=double(q)*q;
        }
        // The CPU-world recenter already requires this tighter range;
        // reject at publication rather than emitting a doomed pose packet.
        if(!std::isfinite(norm)||norm<0.99||norm>1.01)
            return {StereoRigRejection::InvalidEye,0.f};
        norms[eye_index]=norm;
        for(const float p:eye.position_m){
            if(!std::isfinite(p)||std::abs(p)>10000.f)
                return {StereoRigRejection::InvalidEye,0.f};
        }
        const auto& f=eye.fov_left_right_up_down;
        for(const float angle:f){
            if(!std::isfinite(angle)||std::abs(angle)>=1.55f)
                return {StereoRigRejection::UnphysicalFov,0.f};
        }
        // Standard OpenXR primary stereo left/right/up/down convention.
        // Positive-only/mirrored HUD-style projections are not eye views.
        if(!(f[0]<0.f && f[1]>0.f && f[2]>0.f && f[3]<0.f))
            return {StereoRigRejection::UnphysicalFov,0.f};
        horizontal[eye_index]=double(f[1])-f[0];
        vertical[eye_index]=double(f[2])-f[3];
        if(horizontal[eye_index]<0.2||vertical[eye_index]<0.2)
            return {StereoRigRejection::UnphysicalFov,0.f};
    }
    // Canted stereo lenses are legitimate, but two eyes looking opposite
    // ways cannot describe one tracked HMD pose. q and -q are identical:
    // compare the normalized absolute quaternion dot, with a generous
    // 30-degree maximum relative-eye rotation (cos(15 degrees)).
    const auto& q0=source.eyes[0].orientation_xyzw;
    const auto& q1=source.eyes[1].orientation_xyzw;
    double dot=0.;
    for(int i=0;i<4;++i)dot+=double(q0[i])*q1[i];
    constexpr double kMinCosHalfAngle=0.9659258262890683;
    if(dot*dot < kMinCosHalfAngle*kMinCosHalfAngle*norms[0]*norms[1])
        return {StereoRigRejection::DivergentEyeRotation,0.f};
    // Large inter-eye projection mismatch can label two unrelated source
    // cameras as an independent binocular pair. Preserve asymmetric and
    // canted real headset lenses; exclude only >55% span disagreement.
    for(const auto* spans : {horizontal,vertical}) {
        const double larger=(std::max)(spans[0],spans[1]);
        if(std::abs(spans[0]-spans[1])>0.55*larger)
            return {StereoRigRejection::MismatchedEyeFov,0.f};
    }
    const auto& left=source.eyes[0];
    const auto& right=source.eyes[1];
    const double dx=double(right.position_m[0])-left.position_m[0];
    const double dy=double(right.position_m[1])-left.position_m[1];
    const double dz=double(right.position_m[2])-left.position_m[2];
    const double ipd=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(ipd)||ipd<0.02||ipd>0.11)
        return {StereoRigRejection::InvalidIpd,0.f};
    // The original real D3D9/CPU stereo gate must never swap OpenXR view
    // indices. XR right eye must broadly lie on left-view local +X.
    const double inv_norm=1./norms[0];
    const double x=q0[0],y=q0[1],z=q0[2],w=q0[3];
    const double bx=1.-2.*(y*y+z*z)*inv_norm;
    const double by=2.*(x*y+w*z)*inv_norm;
    const double bz=2.*(x*z-w*y)*inv_norm;
    const double alignment=(dx*bx+dy*by+dz*bz)/ipd;
    if(!std::isfinite(alignment)||alignment<0.5)
        return {StereoRigRejection::ReversedEyeOrder,0.f};
    return {StereoRigRejection::None,static_cast<float>(ipd)};
}
} // namespace m2vr::pose
