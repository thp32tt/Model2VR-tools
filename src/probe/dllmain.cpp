#include "config.h"
#include "hooks.h"
#include "logger.h"
#include "../common/probe_ipc.h"

#include <Windows.h>
#include <filesystem>
#include <string>

namespace {
DWORD WINAPI ProbeThread(void* parameter) {
    const auto self = static_cast<HMODULE>(parameter);
    m2vr::PublishProbeState(m2vr::ProbeState::Loaded, m2vr::ProbeError::None);
    const auto directory = m2vr::ModuleDirectory(self);

    m2vr::ProbeConfig config;
    std::wstring error;
    if (!m2vr::LoadProbeConfig(directory / L"model2vr_probe.ini", config, error)) {
        m2vr::PublishProbeState(m2vr::ProbeState::Failed, m2vr::ProbeError::Config);
        return 0;
    }
    if (!config.enabled) {
        m2vr::PublishProbeState(m2vr::ProbeState::Failed, m2vr::ProbeError::Disabled);
        return 0;
    }

    auto log_path = config.log_file;
    if (log_path.is_relative()) log_path = directory / log_path;
    if (!m2vr::OpenLog(log_path)) {
        m2vr::PublishProbeState(m2vr::ProbeState::Failed, m2vr::ProbeError::LogOpen);
        return 0;
    }
    m2vr::LogEvent("probe_start");

    HMODULE exe = GetModuleHandleW(nullptr);
    if (!exe) {
        m2vr::LogEvent("exe_module_missing");
        m2vr::PublishProbeState(m2vr::ProbeState::Failed, m2vr::ProbeError::Executable);
        m2vr::CloseLog();
        return 0;
    }
    if (!m2vr::InstallProbeHooks(exe, config)) {
        m2vr::LogEvent("hook_install_failed");
        m2vr::PublishProbeState(m2vr::ProbeState::Failed, m2vr::ProbeError::HookInstall);
        m2vr::CloseLog();
        return 0;
    }
    m2vr::LogEvent("probe_ready");
    m2vr::PublishProbeState(m2vr::ProbeState::Ready, m2vr::ProbeError::None);
    return 0;
}
}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        // Worker starts after loader lock is released. The launcher waits for READY.
        HANDLE thread = CreateThread(nullptr, 0, ProbeThread, module, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    // Process-lifetime hooks: no dynamic FreeLibrary/unload support in M01.
    return TRUE;
}
