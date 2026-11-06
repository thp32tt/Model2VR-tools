#include "../host/xr_source_eye_coherence.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <utility>
#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"XR source eye coherence line %d: %s\n",__LINE__,#c);std::exit(1);} } while(0)
int main() {
    using namespace m2vr;
    using host::ExactSourceEyeViews;
    std::array<XrView,2> views{XrView{XR_TYPE_VIEW},XrView{XR_TYPE_VIEW}};
    for(std::size_t i=0;i<2;++i) {
        auto& v=views[i];
        v.pose.position.x=i?0.032f:-0.032f;
        v.pose.position.y=1.2f;
        v.pose.position.z=-0.4f;
        v.pose.orientation.w=1.f;
        v.fov.angleLeft=-0.8f;
        v.fov.angleRight=0.8f;
        v.fov.angleUp=0.8f;
        v.fov.angleDown=-0.8f;
    }
    pose::Frame packet{};
    packet.sequence=10;
    packet.reserved=1;
    packet.predicted_display_time=1000;
    packet.view_flags=pose::kFullSixDofTracked;
    for(std::size_t i=0;i<2;++i) {
        auto& p=packet.eyes[i];
        const auto& v=views[i];
        p.position_m[0]=v.pose.position.x;
        p.position_m[1]=v.pose.position.y;
        p.position_m[2]=v.pose.position.z;
        p.orientation_xyzw[0]=v.pose.orientation.x;
        p.orientation_xyzw[1]=v.pose.orientation.y;
        p.orientation_xyzw[2]=v.pose.orientation.z;
        p.orientation_xyzw[3]=v.pose.orientation.w;
        p.fov_left_right_up_down[0]=v.fov.angleLeft;
        p.fov_left_right_up_down[1]=v.fov.angleRight;
        p.fov_left_right_up_down[2]=v.fov.angleUp;
        p.fov_left_right_up_down[3]=v.fov.angleDown;
    }
    CHECK(ExactSourceEyeViews(packet,views));
    auto swapped=views;
    std::swap(swapped[0],swapped[1]);
    CHECK(!ExactSourceEyeViews(packet,swapped));
    auto other=views;
    other[0].pose.position.y+=0.001f;
    CHECK(!ExactSourceEyeViews(packet,other));
    other=views;other[1].pose.orientation.z=0.01f;
    CHECK(!ExactSourceEyeViews(packet,other));
    other=views;other[0].fov.angleRight=0.79f;
    CHECK(!ExactSourceEyeViews(packet,other));
    other=views;other[1].type=XR_TYPE_UNKNOWN;
    CHECK(!ExactSourceEyeViews(packet,other));
    // A second plausible but substituted eye source is not the same
    // physical sample, even with matching sequence/time and tracked flags.
    auto altered=packet;
    altered.eyes[1].position_m[2]+=0.01f;
    CHECK(!ExactSourceEyeViews(altered,views));
    altered=packet;altered.eyes[0].fov_left_right_up_down[0]=-0.79f;
    CHECK(!ExactSourceEyeViews(altered,views));
    altered=packet;altered.eyes[0].orientation_xyzw[1]=-0.f;
    CHECK(!ExactSourceEyeViews(altered,views));
    CHECK(ExactSourceEyeViews(packet,views));
    return 0;
}
