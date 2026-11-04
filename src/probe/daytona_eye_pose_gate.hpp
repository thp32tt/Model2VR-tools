#pragma once
// Daytona real OpenXR pose admission for the original game's independent
// GPU-eye renderer. Does not imply that emulator camera tracking is calibrated.
#include "../common/stereo_pose_ipc.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace m2vr::daytona {
// Signed/near-zero shader POSITION.W is not a safe source for a
// simple left/right clip-X translation: it can reverse apparent disparity
// or magnify it arbitrarily near W=0. Do not modify these triangles.
// This is an empirical guard, not proof that W is fully calibrated depth.
inline bool SafeStereoHomogeneousW(float w) noexcept {
    return std::isfinite(w) && w>=0.05f && w<=65536.f;
}
struct EyePoseAdmission {
    bool valid=false;
    float ipd_scale=1.f; // relative to 64mm; used only for experimental disparity
    std::uint64_t sequence=0;
};
// Check one coherent, recently published HMD stereo pair exactly once per
// original game frame. No mono-cloned or stale OpenXR pair may pass.
inline EyePoseAdmission AdmitLiveEyePose(
    const pose::Frame& frame,std::uint64_t now_qpc,
    std::uint64_t qpc_hz) noexcept {
    EyePoseAdmission out{};
    if(!frame.sequence || !frame.published_qpc || !qpc_hz ||
       now_qpc<frame.published_qpc ||
       now_qpc-frame.published_qpc>qpc_hz/4u ||
       (frame.view_flags&(pose::kOrientationValid|pose::kPositionValid)) !=
         (pose::kOrientationValid|pose::kPositionValid))
        return out;
    for(const auto& eye:frame.eyes) {
        float qlen=0.f;
        for(float q:eye.orientation_xyzw) {
            if(!std::isfinite(q))return {};
            qlen+=q*q;
        }
        if(!std::isfinite(qlen)||qlen<0.98f||qlen>1.02f)
            return {};
        for(float pos:eye.position_m)if(!std::isfinite(pos))return {};
        const auto& fov=eye.fov_left_right_up_down;
        for(float angle:fov)
            if(!std::isfinite(angle) || std::abs(angle)>1.57f)return {};
        if(fov[1]-fov[0]<0.2f || fov[2]-fov[3]<0.2f)return {};
    }
    const auto& l=frame.eyes[0],&r=frame.eyes[1];
    const float dx=l.position_m[0]-r.position_m[0];
    const float dy=l.position_m[1]-r.position_m[1];
    const float dz=l.position_m[2]-r.position_m[2];
    const float ipd=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(ipd)||ipd<0.02f||ipd>0.11f)
        return {}; // no bogus stereo with identical eye poses
    // XR_PRIMARY_STEREO view index 0 is left, index 1 right. A malformed
    // packet can have plausible IPD length but reversed/swapped view order.
    // Verify the right-eye baseline points broadly along the left view's
    // tracked local +X axis in XR LOCAL coordinates (allow for canted HMDs).
    const auto& q=l.orientation_xyzw;
    const float bx=1.f-2.f*(q[1]*q[1]+q[2]*q[2]);
    const float by=2.f*(q[0]*q[1]+q[3]*q[2]);
    const float bz=2.f*(q[0]*q[2]-q[3]*q[1]);
    const float alignment=((-dx)*bx+(-dy)*by+(-dz)*bz)/ipd;
    if(!std::isfinite(alignment)||alignment<0.5f)
        return {}; // reversed or non-horizontal binocular pair
    out.valid=true;
    out.sequence=frame.sequence;
    out.ipd_scale=(std::clamp)(ipd/0.064f,0.6f,1.5f);
    return out;
}
// The separately authenticated head-relative CPU-world path must never
// treat a last-known VALID but untracked HMD view as a live 6DoF camera.
// The less strict GPU legacy shift gate remains ABI-compatible.
inline EyePoseAdmission AdmitTrackedWorldEyePose(
    const pose::Frame& frame,std::uint64_t now_qpc,
    std::uint64_t qpc_hz) noexcept {
    if((frame.view_flags & pose::kFullSixDofTracked)!=
        pose::kFullSixDofTracked)return {};
    return AdmitLiveEyePose(frame,now_qpc,qpc_hz);
}
} // namespace m2vr::daytona
