#pragma once
// The native Windows emulator configuration/ROM-selection GUI is a desktop
// HWND, not part of the game's x86 D3D9 frame stream. Capture only its visible
// top-level window into the host's existing 1024x1024 mono OpenXR quad.
#include <Windows.h>
#include <cstdint>
#include <vector>
namespace m2vr::host {
class EmulatorWindowCapture final {
public:
    bool Update(std::uint32_t emulator_pid);
    void Invalidate() noexcept {
        ready_=false; window_=nullptr; next_capture_ms_=0;
        reticle_visible_=false;
    }
    // Controller input never touches the game; the Windows source HWND must
    // still be the selected, foreground emulator window when a click occurs.
    bool DeliverControllerClick(std::uint32_t pid,std::uint32_t window_x,
                                std::uint32_t window_y);
    void SetControllerReticle(bool visible,std::uint32_t texture_x,
                             std::uint32_t texture_y) noexcept {
        reticle_visible_=visible;
        reticle_x_=texture_x;
        reticle_y_=texture_y;
    }
    bool Ready() const noexcept {return ready_;}
    std::uint64_t Serial() const noexcept {return serial_;}
    HWND Window() const noexcept {return window_;}
    const std::vector<std::uint8_t>& Rgba() const noexcept {return rgba_;}
private:
    bool Capture(HWND hwnd);
    HWND window_=nullptr;
    bool ready_=false;
    std::uint64_t serial_=0;
    std::uint64_t next_capture_ms_=0;
    std::vector<std::uint8_t> rgba_;
    bool reticle_visible_=false;
    std::uint32_t reticle_x_=0,reticle_y_=0;
};
} // namespace m2vr::host
