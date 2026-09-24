#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

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
                WaitForSingleObject(thread, 10000);
                DWORD module_result = 0;
                if (GetExitCodeThread(thread, &module_result) && module_result != 0) {
                    ok = true;
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

    if (!InjectDll(pi.hProcess, probe)) {
        std::wcerr << L"DLL injection failed.\n";
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 7;
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
