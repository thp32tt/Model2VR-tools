#pragma once
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace m2vr::ffbui {
struct IniEdit {
    std::wstring section;
    std::wstring key;
    std::optional<std::wstring> before; // nullopt = key was absent
    std::wstring after;
};
enum class IniSaveStatus { Saved, NoChanges, WrongGame, Conflict, IoError };
std::optional<std::wstring> ReadIniSetting(const std::filesystem::path& path,
                                           const wchar_t* section,
                                           const wchar_t* key);
IniSaveStatus SaveModel2Ini(const std::filesystem::path& ini,
                            std::span<const IniEdit> edits);
// Never edits an active haptic device, game process, or other DLL.
}