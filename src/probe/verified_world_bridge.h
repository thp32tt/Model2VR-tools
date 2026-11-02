#pragma once
// Stable external renderer-adapter contract for verified Model2 world triangles.
// The GPU probe does not automatically label a D3D9 draw as world geometry.
#include <Windows.h>
#include <cstdint>

namespace m2vr::vrbridge {
enum WorldProof : std::uint32_t {
    WorldCpuPass=1u<<0, GameplayActive=1u<<1,
    RenderTargetKnown=1u<<2, MainBackbuffer=1u<<3,
    DepthEnabled=1u<<4, NoStereoReplay=1u<<5,
    NoStateBlock=1u<<6, NoMrt=1u<<7,
    NoOcclusionQuery=1u<<8, ProjectionCalibrated=1u<<9
};
inline constexpr std::uint32_t kRequiredWorldProof =
    WorldCpuPass|GameplayActive|RenderTargetKnown|MainBackbuffer|
    DepthEnabled|NoStereoReplay|NoStateBlock|NoMrt|
    NoOcclusionQuery|ProjectionCalibrated;
// Source adapter must *prove* each bit before calling. Public probe does
// not set these bits or enable the API by itself.
}
extern "C" {
__declspec(dllexport) BOOL __stdcall Model2VR_BeginVerifiedWorldFrame(
    unsigned long long emulated_frame_id,
    const float* calibrated_view_to_xr_3x3,
    float game_units_per_meter,
    float near_plane_m,
    unsigned int verified_world_proof);
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedWorldTriangles(
    const float* view_space_xyz, unsigned int vertex_count,
    const std::uint32_t* triangle_indices, unsigned int index_count,
    const std::uint32_t* triangle_colors_rgb);
// Optional verified native world texture stage. Interleaved UVs are
// per source vertex, texture is tightly-packed RGBA8 (no palette inference).
// Supports clamp + nearest sampling and alpha test, not translucent blending.
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedTexturedTriangles(
    const float* view_space_xyz, const float* interleaved_uv,
    unsigned int vertex_count,
    const std::uint32_t* triangle_indices, unsigned int index_count,
    const unsigned char* texture_rgba,
    unsigned int texture_width, unsigned int texture_height,
    unsigned int texture_pitch, unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_EndVerifiedWorldFrame();
__declspec(dllexport) void __stdcall Model2VR_AbortVerifiedWorldFrame();
}
