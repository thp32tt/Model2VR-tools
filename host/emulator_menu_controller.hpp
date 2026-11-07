#pragma once
// Optional OpenXR actions for the *desktop emulator menu only*. The native
// game's D3D9 source, controller/FFB bindings and VR camera are unaffected.
#include <Windows.h>
#include <openxr/openxr.h>
#include <array>
#include <cstdint>
#include "emulator_window_capture.hpp"
#include "../src/common/daytona_mono_quad_anchor.hpp"
namespace m2vr::host {
class EmulatorMenuController final {
public:
    bool Initialize(XrInstance instance,XrSession session);
    void Shutdown() noexcept;
    bool Active() const noexcept {return active_;}
    bool RayOnMenu() const noexcept {return ray_on_menu_;}
    void Poll(XrSession session,XrSpace local_space,XrTime predicted_time,
              bool focused,bool menu_visible,std::uint32_t pid,
              const monoquad::Placement& quad,EmulatorWindowCapture& capture,
              std::uint64_t submitted_menu_capture_serial);
private:
    XrInstance instance_=XR_NULL_HANDLE;
    XrActionSet set_=XR_NULL_HANDLE;
    XrAction aim_=XR_NULL_HANDLE;
    XrAction select_=XR_NULL_HANDLE;
    std::array<XrPath,2> hand_paths_{XR_NULL_PATH,XR_NULL_PATH};
    std::array<XrSpace,2> aim_spaces_{XR_NULL_HANDLE,XR_NULL_HANDLE};
    bool active_=false;
    bool ray_on_menu_=false;
};
} // namespace m2vr::host
