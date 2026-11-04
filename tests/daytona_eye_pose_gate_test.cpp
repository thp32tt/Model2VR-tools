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
static m2vr::pose::Frame ValidTestPose(){
    m2vr::pose::Frame f{};
    f.sequence=15;
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
    f=ValidTestPose();CHECK(!AdmitLiveEyePose(f,now+hz,hz).valid);
    f=ValidTestPose();CHECK(!AdmitLiveEyePose(f,now,0).valid);
    f=ValidTestPose();CHECK(!AdmitLiveEyePose(f,900000000ull,hz).valid);
    return 0;
}
