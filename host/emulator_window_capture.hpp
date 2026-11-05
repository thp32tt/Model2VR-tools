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
};
} // namespace m2vr::host
