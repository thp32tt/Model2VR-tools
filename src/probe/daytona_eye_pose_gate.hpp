#pragma once
// Daytona real OpenXR pose admission for the original game's independent
// GPU-eye renderer. Does not imply that emulator camera tracking is calibrated.
#include "../common/stereo_pose_ipc.h"
#include "../common/stereo_eye_rig_geometry.hpp"
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
// The Win32 source-to-Win64 GameStereo IPC carries a bounded uint32 pose
// tag. Never wrap a 64-bit HMD sequence into a different earlier pose.
inline constexpr bool CanTagGameStereoPose(std::uint64_t sequence) noexcept {
    return sequence!=0 && sequence<=0xffffffffull;
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
    const auto rig=pose::CheckStereoRigGeometry(frame);
    if(!rig.valid())return out;
    out.valid=true;
    out.sequence=frame.sequence;
    out.ipd_scale=(std::clamp)(rig.ipd_m/0.064f,0.6f,1.5f);
    return out;
}
// The separately authenticated head-relative CPU-world path must never
// treat a last-known VALID but untracked HMD view as a live 6DoF camera.
// The less strict GPU legacy shift gate remains ABI-compatible.
inline EyePoseAdmission AdmitTrackedWorldEyePose(
    const pose::Frame& frame,std::uint64_t now_qpc,
    std::uint64_t qpc_hz) noexcept {
    if(!frame.reserved ||
       (frame.view_flags & pose::kFullSixDofTracked)!=
        pose::kFullSixDofTracked)return {};
    return AdmitLiveEyePose(frame,now_qpc,qpc_hz);
}
} // namespace m2vr::daytona
