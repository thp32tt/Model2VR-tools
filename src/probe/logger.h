#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace m2vr {

bool OpenLog(const std::filesystem::path& path);
void CloseLog();
void LogEvent(
    std::string_view event,
    std::uint64_t a0 = 0,
    std::uint64_t a1 = 0,
    std::uint64_t a2 = 0,
    std::uint64_t a3 = 0);

}  // namespace m2vr
