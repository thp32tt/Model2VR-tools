#include "config.h"

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <limits>

namespace m2vr {
namespace {

std::wstring ReadIniString(
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* fallback,
    const std::filesystem::path& file) {
    wchar_t buffer[2048]{};
    GetPrivateProfileStringW(
        section,
        key,
        fallback,
        buffer,
        static_cast<DWORD>(std::size(buffer)),
        file.c_str());
    return buffer;
}

bool ParseBool(const std::wstring& value) {
    std::wstring v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return v == L"1" || v == L"true" || v == L"yes" || v == L"on";
}

bool ParseU32(const std::wstring& value, std::uint32_t& out) {
    if (value.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(value.c_str(), &end, 0);
    if (end == value.c_str() || *end != L'\0' || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    out = static_cast<std::uint32_t>(parsed);
    return true;
}

int HexNibble(wchar_t c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'a' && c <= L'f') return 10 + (c - L'a');
    if (c >= L'A' && c <= L'F') return 10 + (c - L'A');
    return -1;
}

bool ParseHexBytes(const std::wstring& value, std::vector<std::uint8_t>& out) {
    out.clear();
    int high = -1;
    for (wchar_t c : value) {
        if (std::iswspace(c) || c == L':' || c == L'-') {
            continue;
        }
        const int n = HexNibble(c);
        if (n < 0) {
            return false;
        }
        if (high < 0) {
            high = n;
        } else {
            out.push_back(static_cast<std::uint8_t>((high << 4) | n));
            high = -1;
        }
    }
    return high < 0 && !out.empty();
}

bool LoadHook(
    const wchar_t* section,
    const std::filesystem::path& file,
    HookSpec& out,
    std::wstring& error) {
    out.enabled = ParseBool(ReadIniString(section, L"enabled", L"0", file));
    if (!out.enabled) {
        return true;
    }

    if (!ParseU32(ReadIniString(section, L"rva", L"", file), out.rva) || out.rva == 0) {
        error = std::wstring(section) + L": missing/invalid rva";
        return false;
    }

    if (!ParseHexBytes(ReadIniString(section, L"signature", L"", file), out.signature)) {
        error = std::wstring(section) + L": missing/invalid signature";
        return false;
    }
    return true;
}

std::uint32_t ReadU32(
    const wchar_t* section,
    const wchar_t* key,
    std::uint32_t fallback,
    const std::filesystem::path& file) {
    std::uint32_t value = fallback;
    ParseU32(ReadIniString(section, key, L"", file), value);
    return value;
}

}  // namespace

std::filesystem::path ModuleDirectory(void* module_handle) {
    wchar_t path[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(static_cast<HMODULE>(module_handle), path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(path).parent_path();
}

bool LoadProbeConfig(const std::filesystem::path& path, ProbeConfig& out, std::wstring& error) {
    if (!std::filesystem::exists(path)) {
        error = L"config file not found: " + path.wstring();
        return false;
    }

    out.enabled = ParseBool(ReadIniString(L"probe", L"enabled", L"0", path));
    out.transform_sample_every = std::max<std::uint32_t>(
        1, ReadU32(L"probe", L"transform_sample_every", 256, path));
    out.project_sample_every = std::max<std::uint32_t>(
        1, ReadU32(L"probe", L"project_sample_every", 256, path));
    out.ffb_sample_every = std::max<std::uint32_t>(
        1, ReadU32(L"probe", L"ffb_sample_every", 1, path));

    std::wstring log_file = ReadIniString(L"probe", L"log_file", L"model2vr_probe.csv", path);
    out.log_file = log_file;

    if (!LoadHook(L"hook.transform_point", path, out.transform_point, error)) return false;
    if (!LoadHook(L"hook.project_vertex", path, out.project_vertex, error)) return false;
    if (!LoadHook(L"hook.ffb_dispatch", path, out.ffb_dispatch, error)) return false;

    out.active_matrix_ptr_rva = ReadU32(L"state", L"active_matrix_ptr_rva", 0, path);
    out.geometry_stream_ptr_rva = ReadU32(L"state", L"geometry_stream_ptr_rva", 0, path);
    out.ffb_backend_mode_rva = ReadU32(L"state", L"ffb_backend_mode_rva", 0, path);
    out.ffb_raw_mirror_rva = ReadU32(L"state", L"ffb_raw_mirror_rva", 0, path);

    return true;
}

}  // namespace m2vr
