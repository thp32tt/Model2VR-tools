#include <Windows.h>
#include "../common/probe_ipc.h"
#include "../common/daytona_host_ready.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

class ProbeHandshake {
public:
    explicit ProbeHandshake(DWORD pid) {
        const auto map_name = m2vr::ProbeObjectName(L"Status", pid);
        const auto event_name = m2vr::ProbeObjectName(L"Event", pid);
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                      sizeof(m2vr::ProbeShared), map_name.c_str());
        if (!mapping_) return;
        state_ = static_cast<m2vr::ProbeShared*>(MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0,
                                                              sizeof(m2vr::ProbeShared)));
        if (!state_) return;
        state_->magic = m2vr::kProbeMagic;
        state_->version = 1;
        state_->state = static_cast<LONG>(m2vr::ProbeState::Waiting);
        state_->error = 0;
        event_ = CreateEventW(nullptr, TRUE, FALSE, event_name.c_str());
    }
    ~ProbeHandshake() {
        if (state_) UnmapViewOfFile(state_);
        if (mapping_) CloseHandle(mapping_);
        if (event_) CloseHandle(event_);
    }
    ProbeHandshake(const ProbeHandshake&) = delete;
    ProbeHandshake& operator=(const ProbeHandshake&) = delete;
    bool valid() const { return state_ && event_; }
    bool wait_ready() const {
        if (!valid()) return false;
        DWORD result = WaitForSingleObject(event_, 15000);
        if (result != WAIT_OBJECT_0) {
            std::wcerr << L"Probe initialization timeout/failure: " << result << L"\n";
            return false;
        }
        MemoryBarrier();
        if (state_->state != static_cast<LONG>(m2vr::ProbeState::Ready)) {
            std::wcerr << L"Probe initialization rejected, code=" << state_->error << L"\n";
            return false;
        }
        return true;
    }
private:
    HANDLE mapping_ = nullptr;
    HANDLE event_ = nullptr;
    m2vr::ProbeShared* state_ = nullptr;
};

std::wstring QuoteArg(const std::wstring& arg) {
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }

    std::wstring out = L"\"";
    unsigned backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'\"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        out.append(backslashes, L'\\');
        backslashes = 0;
        out.push_back(c);
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'\"');
    return out;
}

std::filesystem::path SelfDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path HostLogPath(DWORD emulator_pid) {
    const auto logs = SelfDirectory() / L"logs";
    std::error_code ec;
    std::filesystem::create_directories(logs, ec);
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t name[160]{};
    swprintf_s(name, L"daytona_vr_host_%04hu%02hu%02hu_%02hu%02hu%02hu_%lu.log",
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
               now.wSecond, emulator_pid);
    return logs / name;
}

bool StartOpenXRHostAndWait(DWORD emulator_pid,
                           DWORD timeout_ms,
                           HANDLE& host_process,
                           std::filesystem::path& host_log,
                           std::wstring& error) {
    host_process = nullptr;
    host_log = HostLogPath(emulator_pid);
    const auto xr = SelfDirectory() / L"Model2VROpenXRHost.exe";
    if (!std::filesystem::exists(xr)) {
        error = L"OpenXR stereo host missing beside loader";
        return false;
    }

    const auto ready_name = m2vr::hostready::EventName(emulator_pid);
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, ready_name.c_str());
    if (!ready) {
        error = L"Cannot create OpenXR ready event, error=" + std::to_wstring(GetLastError());
        return false;
    }
    ResetEvent(ready);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE log = CreateFileW(host_log.c_str(), GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) {
        error = L"Cannot create OpenXR host log: " + host_log.wstring();
        CloseHandle(ready);
        return false;
    }

    const std::wstring cmd = QuoteArg(xr.wstring()) + L" 0 " +
        std::to_wstring(emulator_pid);
    std::vector<wchar_t> args(cmd.begin(), cmd.end());
    args.push_back(L'\0');
    STARTUPINFOW host_si{};
    host_si.cb = sizeof(host_si);
    host_si.dwFlags = STARTF_USESTDHANDLES;
    host_si.hStdOutput = log;
    host_si.hStdError = log;
    host_si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION host_pi{};
    const auto folder = SelfDirectory().wstring();
    const BOOL started = CreateProcessW(
        xr.c_str(), args.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, folder.c_str(), &host_si, &host_pi);
    CloseHandle(log);
    if (!started) {
        error = L"OpenXR stereo host could not start, error=" +
                std::to_wstring(GetLastError());
        CloseHandle(ready);
        return false;
    }
    CloseHandle(host_pi.hThread);

    HANDLE waits[2]{ready, host_pi.hProcess};
    const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, timeout_ms);
    if (wait == WAIT_OBJECT_0) {
        host_process = host_pi.hProcess;
        CloseHandle(ready);
        std::wcout << L"OpenXR headset/tracking ready; host log: "
                   << host_log << L"\n";
        return true;
    }

    DWORD exit_code = STILL_ACTIVE;
    GetExitCodeProcess(host_pi.hProcess, &exit_code);
    if (wait == WAIT_OBJECT_0 + 1) {
        error = L"OpenXR host exited before headset/tracking became ready; code=" +
                std::to_wstring(exit_code) + L"; see " + host_log.wstring();
    } else if (wait == WAIT_TIMEOUT) {
        error = L"OpenXR headset/tracking readiness timed out after " +
                std::to_wstring(timeout_ms) + L" ms; see " + host_log.wstring();
    } else {
        error = L"OpenXR host readiness wait failed, error=" +
                std::to_wstring(GetLastError()) + L"; see " + host_log.wstring();
    }
    if (exit_code == STILL_ACTIVE) TerminateProcess(host_pi.hProcess, 2);
    CloseHandle(host_pi.hProcess);
    CloseHandle(ready);
    return false;
}

// Control panel is a separate *UI* process; never initializes a wheel,
// DirectInput device or second FFB effect router. The plugin's dinput8.dll
// remains the only FFB owner and is loaded by the emulator's normal path.
void StartFfbControlPanel(DWORD emulator_pid,const std::filesystem::path& game_dir) {
    const auto ui=SelfDirectory()/L"Model2VRControlPanel.exe";
    if(!std::filesystem::exists(ui))return;
    if(!std::filesystem::exists(game_dir/L"dinput8.dll") ||
       !std::filesystem::exists(game_dir/L"FFBPlugin.ini")){
        std::wcerr<<L"FFB companion not installed beside emulator; "
                    L"control panel skipped.\n";
        return;
    }
    const std::wstring cmd=QuoteArg(ui.wstring())+L" "+
        std::to_wstring(emulator_pid)+L" "+QuoteArg(game_dir.wstring());
    std::vector<wchar_t> command(cmd.begin(),cmd.end());
    command.push_back(L'\0');
    STARTUPINFOW si{}; si.cb=sizeof(si);
    PROCESS_INFORMATION proc{};
    if(CreateProcessW(ui.c_str(),command.data(),nullptr,nullptr,FALSE,0,
                      nullptr,game_dir.c_str(),&si,&proc)){
        CloseHandle(proc.hThread);
        CloseHandle(proc.hProcess);
    } else {
        std::wcerr<<L"FFB control panel could not start: "<<GetLastError()<<L"\n";
    }
}

bool InjectDll(HANDLE process, const std::filesystem::path& dll) {
    const std::wstring dll_path = std::filesystem::absolute(dll).wstring();
    const SIZE_T bytes = (dll_path.size() + 1) * sizeof(wchar_t);

    void* remote = VirtualAllocEx(
        process,
        nullptr,
        bytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (!remote) {
        return false;
    }

    bool ok = false;
    SIZE_T written = 0;
    if (WriteProcessMemory(process, remote, dll_path.c_str(), bytes, &written) && written == bytes) {
        auto* kernel32 = GetModuleHandleW(L"kernel32.dll");
        auto* load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(kernel32, "LoadLibraryW"));
        if (load_library) {
            HANDLE thread = CreateRemoteThread(
                process,
                nullptr,
                0,
                load_library,
                remote,
                0,
                nullptr);
            if (thread) {
                const DWORD wait = WaitForSingleObject(thread, 10000);
                if (wait == WAIT_OBJECT_0) {
                    DWORD exit_code = 0;
                    if (GetExitCodeThread(thread, &exit_code) &&
                        exit_code != 0 && exit_code != STILL_ACTIVE)
                        ok = true;
                }
                // Never release the remote parameter buffer while the remote thread
                // could still be using it. The suspended process is terminated on error.
                if (wait != WAIT_OBJECT_0) {
                    CloseHandle(thread);
                    return false;
                }
                CloseHandle(thread);
            }
        }
    }

    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return ok;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcerr << L"Usage: Model2VRLoader.exe <emulator.exe> [args...]\n";
        return 2;
    }

    const std::filesystem::path emulator = std::filesystem::absolute(argv[1]);
    const auto probe = SelfDirectory() / L"Model2VRProbe.dll";
    const auto config = SelfDirectory() / L"model2vr_probe.ini";

    if (!std::filesystem::exists(emulator)) {
        std::wcerr << L"Emulator not found: " << emulator << L"\n";
        return 3;
    }
    if (!std::filesystem::exists(probe)) {
        std::wcerr << L"Probe DLL not found: " << probe << L"\n";
        return 4;
    }
    if (!std::filesystem::exists(config)) {
        std::wcerr << L"Probe config not found: " << config << L"\n";
        return 5;
    }

    std::wstring command = QuoteArg(emulator.wstring());
    for (int i = 2; i < argc; ++i) {
        command.push_back(L' ');
        command += QuoteArg(argv[i]);
    }
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const auto working_dir = emulator.parent_path().wstring();
    if (!CreateProcessW(
            emulator.c_str(),
            mutable_command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED,
            nullptr,
            working_dir.c_str(),
            &si,
            &pi)) {
        std::wcerr << L"CreateProcess failed: " << GetLastError() << L"\n";
        return 6;
    }

    ProbeHandshake handshake(pi.dwProcessId);
    if (!handshake.valid() || !InjectDll(pi.hProcess, probe) || !handshake.wait_ready()) {
        std::wcerr << L"Probe loading or initialization failed; emulator will not start.\n";
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 7;
    }

    HANDLE host_process = nullptr;
    std::filesystem::path host_log;
    if (GetPrivateProfileIntW(L"probe", L"launch_openxr_stereo_host",
                              0, config.c_str()) != 0) {
        const DWORD configured_timeout = static_cast<DWORD>(GetPrivateProfileIntW(
            L"probe", L"openxr_host_ready_timeout_ms", 15000, config.c_str()));
        const DWORD timeout_ms = (std::clamp)(configured_timeout, 3000u, 60000u);
        const bool require_ready = GetPrivateProfileIntW(
            L"probe", L"require_openxr_host_ready", 1, config.c_str()) != 0;
        std::wstring host_error;
        if (!StartOpenXRHostAndWait(pi.dwProcessId, timeout_ms,
                                    host_process, host_log, host_error)) {
            std::wcerr << host_error << L"\n";
            if (require_ready) {
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                return 9;
            }
            std::wcerr << L"Continuing without VR because require_openxr_host_ready=0\n";
        }
    }

    // Do not let the desktop game silently continue in 2D while the requested
    // VR host is still failing. For Daytona VR the emulator is resumed only
    // after OpenXR has produced a valid tracked stereo pose.
    if (ResumeThread(pi.hThread) == DWORD(-1)) {
        if (host_process) {
            TerminateProcess(host_process, 3);
            CloseHandle(host_process);
        }
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 8;
    }
    if (host_process) CloseHandle(host_process);

    // Diagnostic-only Daytona sessions must not launch any FFB editor.
    // Preserve existing behavior when this optional INI key is absent.
    if (GetPrivateProfileIntW(L"probe",L"launch_ffb_control_panel",
                              1,config.c_str()) != 0)
        StartFfbControlPanel(pi.dwProcessId, emulator.parent_path());

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
