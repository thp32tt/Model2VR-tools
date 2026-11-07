#pragma once
// Before genuine Daytona game frames exist, show the ENTIRE Windows desktop
// (multi-monitor origin, taskbar, ROM dialogs, physical mouse pointer) in
// the host's existing 1024x1024 head-tracked OpenXR quad.
#include <Windows.h>
#include "../src/common/model2_emulator_window_policy.hpp"
#include <cstdint>
#include <vector>
namespace m2vr::host {
class EmulatorWindowCapture final {
public:
    bool Update(std::uint32_t emulator_pid);
    void Invalidate() noexcept {
        ready_=false; window_=nullptr; next_capture_ms_=0;
        source_bounds_={}; reticle_visible_=false;
        captured_window_rect_={}; captured_ms_=0u;
        click_geometry_valid_=false;
    }
    // Controller input never touches the game; the Windows source HWND must
    // still be the selected, foreground emulator window when a click occurs.
    bool DeliverControllerClick(std::uint32_t pid,std::uint32_t desktop_x,
                                std::uint32_t desktop_y);
    void SetControllerReticle(bool visible,std::uint32_t texture_x,
                             std::uint32_t texture_y) noexcept {
        reticle_visible_=visible;
        reticle_x_=texture_x;
        reticle_y_=texture_y;
    }
    bool Ready() const noexcept {return ready_;}
    std::uint64_t Serial() const noexcept {return serial_;}
    HWND Window() const noexcept {return window_;}
    desktopmenu::DesktopBounds SourceBounds() const noexcept {
        return source_bounds_;
    }
    const std::vector<std::uint8_t>& Rgba() const noexcept {return rgba_;}
private:
    bool Capture(desktopmenu::DesktopBounds desktop);
    bool CapturedClickGeometryLive() const noexcept;
    HWND window_=nullptr;
    desktopmenu::DesktopBounds source_bounds_{};
    desktopmenu::WindowRect captured_window_rect_{};
    std::uint64_t captured_ms_=0u;
    bool click_geometry_valid_=false;
    bool ready_=false;
    std::uint64_t serial_=0;
    std::uint64_t next_capture_ms_=0;
    std::vector<std::uint8_t> rgba_;
    bool reticle_visible_=false;
    std::uint32_t reticle_x_=0,reticle_y_=0;
};
} // namespace m2vr::host
