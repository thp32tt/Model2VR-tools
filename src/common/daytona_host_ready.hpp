#pragma once
// Cross-bitness startup gate shared by Win32 loader and Win64 OpenXR host.
// The event is signaled only after the host has initialized OpenXR and
// published a valid tracked stereo pose for the suspended emulator PID.
#include <Windows.h>
#include <cstdint>
#include <string>

namespace m2vr::hostready {
inline std::wstring EventName(std::uint32_t emulator_pid) {
    return L"Local\\Model2VR_OpenXRTrackedReady_v1_" + std::to_wstring(emulator_pid);
}
inline bool Signal(std::uint32_t emulator_pid) noexcept {
    if (!emulator_pid) return false;
    const auto name = EventName(emulator_pid);
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
    if (!event) return false;
    const bool ok = SetEvent(event) != FALSE;
    CloseHandle(event);
    return ok;
}
}  // namespace m2vr::hostready
