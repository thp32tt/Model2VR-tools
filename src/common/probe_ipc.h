#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace m2vr {
constexpr std::uint32_t kProbeMagic = 0x3256524Du;  // "MRV2"
enum class ProbeState : std::uint32_t {
    Waiting = 0,
    Loaded = 1,
    Ready = 2,
    Failed = 3,
};
enum class ProbeError : std::uint32_t {
    None = 0,
    Config = 1,
    Disabled = 2,
    LogOpen = 3,
    Executable = 4,
    HookInstall = 5,
};

struct ProbeShared {
    std::uint32_t magic;
    volatile LONG state;
    volatile LONG error;
    std::uint32_t version;
};

inline std::wstring ProbeObjectName(const wchar_t* kind, DWORD pid) {
    return L"Local\\Model2VR_" + std::wstring(kind) + L"_" + std::to_wstring(pid);
}
inline bool PublishProbeState(ProbeState state, ProbeError error) {
    const DWORD pid = GetCurrentProcessId();
    const std::wstring map_name = ProbeObjectName(L"Status", pid);
    const std::wstring event_name = ProbeObjectName(L"Event", pid);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, map_name.c_str());
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, event_name.c_str());
    if (!mapping || !event) {
        if (mapping) CloseHandle(mapping);
        if (event) CloseHandle(event);
        return false;
    }
    auto* shared = static_cast<ProbeShared*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(ProbeShared)));
    bool published = false;
    if (shared && shared->magic == kProbeMagic && shared->version == 1) {
        InterlockedExchange(&shared->error, static_cast<LONG>(error));
        InterlockedExchange(&shared->state, static_cast<LONG>(state));
        if (state == ProbeState::Ready || state == ProbeState::Failed)
            SetEvent(event);
        published = true;
    }
    if (shared) UnmapViewOfFile(shared);
    CloseHandle(event);
    CloseHandle(mapping);
    return published;
}
}  // namespace m2vr
