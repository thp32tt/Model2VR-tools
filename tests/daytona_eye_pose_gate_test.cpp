// Headless Windows x86/x64 verification for Quest-pose admission.
// No game EXE, actual HMD, or FFB device is needed.
#include "../src/probe/daytona_eye_pose_gate.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

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
    constexpr std::uint64_t hz=10000000ull;
    constexpr std::uint64_t now=1000200000ull;
    auto f=ValidTestPose();
    auto r=AdmitLiveEyePose(f,now,hz);
    assert(r.valid && r.sequence==15);
    assert(std::abs(r.ipd_scale-1.f)<0.001f);
    f.sequence=0;assert(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.view_flags=m2vr::pose::kOrientationValid;
    assert(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[1].position_m[0]=-0.032f;
    assert(!AdmitLiveEyePose(f,now,hz).valid); // cannot clone mono eyes
    f=ValidTestPose();f.eyes[1].position_m[0]=0.0f;
    r=AdmitLiveEyePose(f,now,hz);
    assert(r.valid && std::abs(r.ipd_scale-0.5f)<0.001f);
    f=ValidTestPose();f.eyes[0].position_m[2]=
        std::numeric_limits<float>::quiet_NaN();
    assert(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[0].orientation_xyzw[3]=0.5f;
    assert(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[0].fov_left_right_up_down[1]=-0.6f;
    assert(!AdmitLiveEyePose(f,now,hz).valid);
    f=ValidTestPose();f.eyes[1].position_m[0]=0.132f;
    assert(!AdmitLiveEyePose(f,now,hz).valid); // implausible IPD
    f=ValidTestPose();assert(!AdmitLiveEyePose(f,now+hz,hz).valid);
    f=ValidTestPose();assert(!AdmitLiveEyePose(f,now,0).valid);
    f=ValidTestPose();assert(!AdmitLiveEyePose(f,900000000ull,hz).valid);
    return 0;
}
