#include "hooks.h"

#include "logger.h"
#include "signature.h"

#include <MinHook.h>

#include <Windows.h>

#include <atomic>
#include <bit>
#include <cstdint>
#include <string>

#if !defined(_MSC_VER) || !defined(_M_IX86)
#error Model2VR register-preserving probe thunks currently require MSVC x86.
#endif

namespace {

m2vr::ProbeConfig g_config;
std::uint8_t* g_exe_base = nullptr;

std::atomic<std::uint64_t> g_transform_counter{0};
std::atomic<std::uint64_t> g_project_counter{0};
std::atomic<std::uint64_t> g_ffb_counter{0};

extern "C" void* g_original_transform_point = nullptr;
extern "C" void* g_original_project_vertex = nullptr;
extern "C" void* g_original_ffb_dispatch = nullptr;

bool ShouldSample(std::atomic<std::uint64_t>& counter, std::uint32_t every) {
    const auto value = counter.fetch_add(1, std::memory_order_relaxed);
    return (value % every) == 0;
}

std::uint32_t SafeReadU32(const std::uint8_t* address) {
    if (!address) return 0;
    __try {
        return *reinterpret_cast<const volatile std::uint32_t*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool SafeReadPoint(const void* pointer, float& x, float& y, float& z) {
    if (!pointer) return false;
    __try {
        const auto* p = static_cast<const volatile float*>(pointer);
        x = p[0];
        y = p[1];
        z = p[2];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::uint64_t FloatBits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t ReadStateRva(std::uint32_t rva) {
    if (!g_exe_base || rva == 0) return 0;
    return SafeReadU32(g_exe_base + rva);
}

extern "C" void __cdecl M2VR_OnTransformPoint(void* point) {
    if (!ShouldSample(g_transform_counter, g_config.transform_sample_every)) {
        return;
    }

    float x = 0, y = 0, z = 0;
    if (!SafeReadPoint(point, x, y, z)) {
        m2vr::LogEvent("transform_read_fault", reinterpret_cast<std::uintptr_t>(point));
        return;
    }

    m2vr::LogEvent(
        "transform_in",
        reinterpret_cast<std::uintptr_t>(point),
        FloatBits(x),
        FloatBits(y),
        FloatBits(z));
}

extern "C" void __cdecl M2VR_OnProjectVertex(void* point) {
    if (!ShouldSample(g_project_counter, g_config.project_sample_every)) {
        return;
    }

    float x = 0, y = 0, z = 0;
    if (!SafeReadPoint(point, x, y, z)) {
        m2vr::LogEvent("project_read_fault", reinterpret_cast<std::uintptr_t>(point));
        return;
    }

    const auto active_matrix = ReadStateRva(g_config.active_matrix_ptr_rva);
    m2vr::LogEvent(
        "project_in",
        FloatBits(x),
        FloatBits(y),
        FloatBits(z),
        active_matrix);
}

extern "C" void __cdecl M2VR_OnFfbDispatch(std::uint32_t ecx_value) {
    if (!ShouldSample(g_ffb_counter, g_config.ffb_sample_every)) {
        return;
    }

    const std::uint32_t command = ecx_value & 0xffu;
    const std::uint32_t backend_mode = ReadStateRva(g_config.ffb_backend_mode_rva);
    const std::uint32_t raw_mirror = ReadStateRva(g_config.ffb_raw_mirror_rva) & 0xffu;
    m2vr::LogEvent("ffb_in", command, backend_mode, raw_mirror, 0);
}

__declspec(naked) void TransformPointDetour() {
    __asm {
        pushfd
        pushad
        push eax
        call M2VR_OnTransformPoint
        add esp, 4
        popad
        popfd
        jmp dword ptr [g_original_transform_point]
    }
}

__declspec(naked) void ProjectVertexDetour() {
    __asm {
        pushfd
        pushad
        push eax
        call M2VR_OnProjectVertex
        add esp, 4
        popad
        popfd
        jmp dword ptr [g_original_project_vertex]
    }
}

__declspec(naked) void FfbDispatchDetour() {
    __asm {
        pushfd
        pushad
        push ecx
        call M2VR_OnFfbDispatch
        add esp, 4
        popad
        popfd
        jmp dword ptr [g_original_ffb_dispatch]
    }
}

bool InstallOne(
    const char* name,
    const m2vr::HookSpec& spec,
    void* detour,
    void** original) {
    if (!spec.enabled) {
        m2vr::LogEvent(std::string("hook_disabled_") + name);
        return true;
    }

    auto* target = g_exe_base + spec.rva;
    std::string signature_error;
    if (!m2vr::VerifySignature(target, spec.signature, signature_error)) {
        m2vr::LogEvent(std::string("signature_fail_") + name, spec.rva);
        return false;
    }

    const MH_STATUS create_status = MH_CreateHook(target, detour, original);
    if (create_status != MH_OK) {
        m2vr::LogEvent(std::string("hook_create_fail_") + name, create_status, spec.rva);
        return false;
    }

    m2vr::LogEvent(std::string("hook_ready_") + name, spec.rva);
    return true;
}

}  // namespace

namespace m2vr {

bool InstallProbeHooks(HMODULE executable, const ProbeConfig& config) {
    g_config = config;
    g_exe_base = reinterpret_cast<std::uint8_t*>(executable);

    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        LogEvent("minhook_init_fail", init);
        return false;
    }

    bool ok = true;
    ok = InstallOne(
             "transform",
             g_config.transform_point,
             reinterpret_cast<void*>(&TransformPointDetour),
             &g_original_transform_point) &&
         ok;
    ok = InstallOne(
             "project",
             g_config.project_vertex,
             reinterpret_cast<void*>(&ProjectVertexDetour),
             &g_original_project_vertex) &&
         ok;
    ok = InstallOne(
             "ffb",
             g_config.ffb_dispatch,
             reinterpret_cast<void*>(&FfbDispatchDetour),
             &g_original_ffb_dispatch) &&
         ok;

    if (!ok) {
        LogEvent("probe_fail_closed");
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        return false;
    }

    const MH_STATUS enable = MH_EnableHook(MH_ALL_HOOKS);
    if (enable != MH_OK) {
        LogEvent("minhook_enable_fail", enable);
        MH_Uninitialize();
        return false;
    }

    LogEvent("probe_hooks_enabled");
    return true;
}

void RemoveProbeHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

}  // namespace m2vr
