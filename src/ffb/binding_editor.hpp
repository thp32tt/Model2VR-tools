#pragma once
#include <Windows.h>
#include <filesystem>
namespace m2vr::ffbui {
void ShowBindingEditor(HWND owner,const std::filesystem::path& ini_path,
                       const std::filesystem::path& game_dir);
}