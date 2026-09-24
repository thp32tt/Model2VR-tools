#include "config.h"
#include "hooks.h"
#include "logger.h"

#include <Windows.h>

#include <filesystem>
#include <string>

namespace {

DWORD WINAPI ProbeThread(void* parameter) {
    auto self = static_cast<HMODULE>(parameter);
    const auto directory = m2vr::ModuleDirectory(self);
    const auto config_path = directory / L"model2vr_probe.ini";

    m2vr::ProbeConfig config;
    std::wstring error;
    if (!m2vr::LoadProbeConfig(config_path, config, error)) {
        return 0;
    }
    if (!config.enabled) {
        return 0;
    }

    std::filesystem::path log_path = config.log_file;
    if (log_path.is_relative()) {
        log_path = directory / log_path;
    }

    if (!m2vr::OpenLog(log_path)) {
        return 0;
    }

    m2vr::LogEvent("probe_start");

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!executable) {
        m2vr::LogEvent("exe_module_missing");
        return 0;
    }

    if (!m2vr::InstallProbeHooks(executable, config)) {
        m2vr::LogEvent("hook_install_failed");
        return 0;
    }

    m2vr::LogEvent("probe_ready");
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, ProbeThread, module, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
