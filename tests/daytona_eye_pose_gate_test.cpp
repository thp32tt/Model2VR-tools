// Headless Windows x86/x64 verification for Quest-pose admission.
// No game EXE, actual HMD, or FFB device is needed.
#include "../src/probe/daytona_eye_pose_gate.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <limits>

// GitHub Windows Release/CTest defines NDEBUG. The old CHECK() checks
// compiled out entirely, allowing a no-op CI success. Keep runtime checks.
#define CHECK(expr) do { if(!(expr)) { \
    std::fprintf(stderr,"HMD pose gate failed at %d: %s\n",__LINE__,#expr); \
    std::exit(1); } } while(0)
using m2vr::daytona::AdmitLiveEyePose;
using m2vr::daytona::AdmitTrackedWorldEyePose;
static m2vr::pose::Frame ValidTestPose(){
    m2vr::pose::Frame f{};
    f.sequence=15;
    f.predicted_display_time=15000;
    f.reserved=1u;
    f.published_qpc=1000000000ull;
    f.view_flags=m2vr::pose::kOrientationValid |
                 m2vr::pose::kPositionValid;
    for(auto& eye:f.eyes){
        eye.orientation_xyzw[3]=1.f;
        eye.fov_left_right_up_down[0]=-0.8f;
        eye.fov_left_right_up_down[1]=0.8f;
        eye.fov_left_right_up_down[2]=0.8f;
        eye.fov_left_right_up_down[3]=-0.8f;
    }
    f.eyes[0].position_m[0]=-0.032f;
    f.eyes[1].position_m[0]=0.032f;
    return f;
}
int main(){
    using m2vr::daytona::CanTagGameStereoPose;
    CHECK(!CanTagGameStereoPose(0u));
    CHECK(CanTagGameStereoPose(1u));
    CHECK(CanTagGameStereoPose(0xffffffffull));
    CHECK(!CanTagGameStereoPose(0x100000000ull));
    CHECK(!CanTagGameStereoPose(0xffffffffffffffffull));
    using m2vr::daytona::SafeStereoHomogeneousW;
    CHECK(SafeStereoHomogeneousW(98.9f));
    CHECK(SafeStereoHomogeneousW(414.78f));
    CHECK(!SafeStereoHomogeneousW(-40.f));
    CHECK(!SafeStereoHomogeneousW(0.f));
    CHECK(!SafeStereoHomogeneousW(0.01f));
    CHECK(!SafeStereoHomogeneousW(65537.f));
    CHECK(!SafeStereoHomogeneousW(
        std::numeric_limits<float>::quiet_NaN()));
    constexpr std::uint64_t hz=10000000ull;
    constexpr std::uint64_t now=1000200000ull;
    auto f=ValidTestPose();
    auto r=AdmitLiveEyePose(f,now,hz);
    CHECK(r.valid && r.sequence==15);
    // A live GUI pose can remain VALID after tracking is lost, but it
    // must NOT be accepted as a calibrated CPU-world 6DoF source.
    CHECK(!AdmitTrackedWorldEyePose(f,now,hz).valid);
    f.view_flags|=m2vr::pose::kOrientationTracked |
                  m2vr::pose::kPositionTracked;
    CHECK(AdmitTrackedWorldEyePose(f,now,hz).valid);
    // World3D admission cannot trust a tracked geometric pair without
    // physical positive predicted XrTime; legacy GPU fallback still can.
    f.predicted_display_time=0;
    CHECK(!AdmitTrackedWorldEyePose(f,now,hz).valid);
    CHECK(AdmitLiveEyePose(f,now,hz).valid);
    f.predicted_display_time=-1;
    CHECK(!AdmitTrackedWorldEyePose(f,now,hz).valid);
    CHECK(AdmitLiveEyePose(f,now,hz).valid);
    f.predicted_display_time=15000;
    CHECK(AdmitTrackedWorldEyePose(f,now,hz).valid);
    f.reserved=0u;
    CHECK(AdmitLiveEyePose(f,now,hz).valid); // preserve legacy GPU/mono
    CHECK(!AdmitTrackedWorldEyePose(f,now,hz).valid);
    f.reserved=1u;
    CHECK(AdmitTrackedWorldEyePose(f,now,hz).valid);
    f.view_flags&=~m2vr::pose::kPositionTracked;
    CHECK(!AdmitTrackedWorldEyePose(f,now,hz).valid);
    f.view_flags|=m2vr::pose::kPositionTracked;
    f.view_flags&=~m2vr::pose::kOrientationTracked;
    CHECK(!AdmitTrackedWorldEyePose(f,now,hz).valid);
    CHECK(std::abs(r.ipd_scale-1.f)<0.001f);
    f.sequence=0;CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.view_flags=m2vr::pose::kOrientationValid;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[1].position_m[0]=-0.032f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid); // cannot clone mono eyes
    f=ValidTestPose();f.eyes[1].position_m[0]=0.0f;
    r=AdmitLiveEyePose(f,now,hz);
    CHECK(r.valid && std::abs(r.ipd_scale-0.6f)<0.001f);
    f=ValidTestPose();f.eyes[0].position_m[2]=
        std::numeric_limits<float>::quiet_NaN();
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[0].orientation_xyzw[3]=0.5f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[0].fov_left_right_up_down[1]=-0.6f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[1].position_m[0]=0.132f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid); // implausible IPD
    // A right-to-left baseline has a valid physical magnitude but is
    // incompatible with XR_PRIMARY_STEREO left/right source order.
    f=ValidTestPose();
    f.eyes[0].position_m[0]=0.032f;
    f.eyes[1].position_m[0]=-0.032f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    // Also reject a plausible 64mm vertical baseline.
    f=ValidTestPose();
    f.eyes[0].position_m[0]=0.f;f.eyes[1].position_m[0]=0.f;
    f.eyes[0].position_m[1]=-0.032f;f.eyes[1].position_m[1]=0.032f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    // A 90-degree HMD yaw rotates both eye positions and local X.
    f=ValidTestPose();
    f.eyes[0].orientation_xyzw[1]=0.70710678f;
    f.eyes[0].orientation_xyzw[3]=0.70710678f;
    f.eyes[1].orientation_xyzw[1]=0.70710678f;
    f.eyes[1].orientation_xyzw[3]=0.70710678f;
    f.eyes[0].position_m[0]=f.eyes[1].position_m[0]=0.f;
    f.eyes[0].position_m[2]=0.032f;
    f.eyes[1].position_m[2]=-0.032f;
    CHECK(AdmitLiveEyePose(f,now,hz).valid);
    // A truthful original Win32 CPU-world eye source must inherit one
    // coherent physical XR_PRIMARY_STEREO pair, even with valid tracking.
    // Allow a genuine canted right-eye camera (up to thirty degrees).
    f=ValidTestPose();
    f.eyes[1].orientation_xyzw[1]=std::sin(0.10f);
    f.eyes[1].orientation_xyzw[3]=std::cos(0.10f);
    CHECK(AdmitLiveEyePose(f,now,hz).valid);
    // q and -q describe the SAME orientation, never reject by signed dot.
    f=ValidTestPose();f.eyes[1].orientation_xyzw[3]=-1.f;
    CHECK(AdmitLiveEyePose(f,now,hz).valid);
    // But two mutually divergent cameras are not stereo HMD eyes.
    f=ValidTestPose();
    f.eyes[1].orientation_xyzw[1]=std::sin(0.50f);
    f.eyes[1].orientation_xyzw[3]=std::cos(0.50f);
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    CHECK(m2vr::pose::CheckStereoRigGeometry(f).reason==
          m2vr::pose::StereoRigRejection::DivergentEyeRotation);
    // Reject a wildly asymmetric lens projection and malformed FOV
    // without ever treating different mono cameras as native stereo.
    f=ValidTestPose();
    f.eyes[1].fov_left_right_up_down[0]=-0.25f;
    f.eyes[1].fov_left_right_up_down[1]=0.25f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    CHECK(m2vr::pose::CheckStereoRigGeometry(f).reason==
          m2vr::pose::StereoRigRejection::MismatchedEyeFov);
    f=ValidTestPose();f.eyes[1].fov_left_right_up_down[2]=-0.8f;
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[0].position_m[0]=
        std::numeric_limits<float>::infinity();
    CHECK(!AdmitLiveEyePose(f,now,hz).valid);
    // A huge absolute headset yaw is fine when BOTH eyes share it.
    f=ValidTestPose();
    f.eyes[0].orientation_xyzw[1]=f.eyes[1].orientation_xyzw[1]=0.70710678f;
    f.eyes[0].orientation_xyzw[3]=f.eyes[1].orientation_xyzw[3]=0.70710678f;
    f.eyes[0].position_m[0]=f.eyes[1].position_m[0]=0.f;
    f.eyes[0].position_m[2]=0.032f;f.eyes[1].position_m[2]=-0.032f;
    CHECK(AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();CHECK(!AdmitLiveEyePose(f,now+hz,hz).valid);
    f=ValidTestPose();CHECK(!AdmitLiveEyePose(f,now,0).valid);
    f=ValidTestPose();CHECK(!AdmitLiveEyePose(f,900000000ull,hz).valid);
    return 0;
}
