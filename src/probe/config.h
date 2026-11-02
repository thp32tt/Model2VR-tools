#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace m2vr {

struct HookSpec {
    bool enabled = false;
    std::uint32_t rva = 0;
    std::vector<std::uint8_t> signature;
};

struct ProbeConfig {
    bool enabled = false;
    bool d3d9_read_only_probe = false;
    bool daytona_native_stereo = false; // opt-in experimental D3D9 two-eye replay
    std::uint32_t daytona_eye_shift_milliunits = 100;
    std::uint32_t d3d9_draw_sample_every = 1024; // independent per-frame family samples
    std::filesystem::path log_file;
    std::uint32_t transform_sample_every = 256;
    std::uint32_t project_sample_every = 256;
    std::uint32_t ffb_sample_every = 1;

    HookSpec transform_point;
    HookSpec project_vertex;
    HookSpec ffb_dispatch;

    std::uint32_t active_matrix_ptr_rva = 0;
    std::uint32_t geometry_stream_ptr_rva = 0;
    std::uint32_t ffb_backend_mode_rva = 0;
    std::uint32_t ffb_raw_mirror_rva = 0;
};

std::filesystem::path ModuleDirectory(void* module_handle);
bool LoadProbeConfig(const std::filesystem::path& path, ProbeConfig& out, std::wstring& error);

}  // namespace m2vr
