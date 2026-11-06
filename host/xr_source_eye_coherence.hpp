#pragma once
// Verify that original Win32 pose IPC and the Win64 XrView history hold
// the exact SAME physical xrLocateViews return. No game renderer/ABI change.
#include "../src/common/stereo_pose_ipc.h"
#include <openxr/openxr.h>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
namespace m2vr::host {
inline bool ExactSourceEyeViews(
    const pose::Frame& source,
    const std::array<XrView,2>& actual_views) noexcept {
    for(std::size_t i=0;i<2;++i) {
        const XrView& xr=actual_views[i];
        if(xr.type!=XR_TYPE_VIEW)return false;
        const pose::EyeView& original=source.eyes[i];
        const float expected[11]={
            original.position_m[0],original.position_m[1],
            original.position_m[2],
            original.orientation_xyzw[0],original.orientation_xyzw[1],
            original.orientation_xyzw[2],original.orientation_xyzw[3],
            original.fov_left_right_up_down[0],
            original.fov_left_right_up_down[1],
            original.fov_left_right_up_down[2],
            original.fov_left_right_up_down[3]
        };
        const float actual[11]={
            xr.pose.position.x,xr.pose.position.y,xr.pose.position.z,
            xr.pose.orientation.x,xr.pose.orientation.y,
            xr.pose.orientation.z,xr.pose.orientation.w,
            xr.fov.angleLeft,xr.fov.angleRight,
            xr.fov.angleUp,xr.fov.angleDown
        };
        // Identical source bits, not approximate camera similarities.
        // A substitute pair must not be attributed to a different IPC pose.
        for(std::size_t n=0;n<11;++n)
            if(std::bit_cast<std::uint32_t>(expected[n])!=
               std::bit_cast<std::uint32_t>(actual[n]))return false;
    }
    return true;
}
} // namespace m2vr::host
