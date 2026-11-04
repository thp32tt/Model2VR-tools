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
/* Fully verified source material sampler: 0=clamp/point, 1=wrap/linear.
   Filter argument is 0 for POINT, 1 for LINEAR. U/V arguments are
   0 for CLAMP and 1 for WRAP. Other values fail closed.
   Existing textured entrypoint retains point/clamp ABI unchanged. */
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithSampler(
    const float* view_space_xyz, const float* interleaved_uv,
    unsigned int vertex_count,
    const std::uint32_t* triangle_indices, unsigned int index_count,
    const unsigned char* texture_rgba,
    unsigned int texture_width, unsigned int texture_height,
    unsigned int texture_pitch, unsigned int alpha_cutoff,
    unsigned int filter_mode, unsigned int address_u, unsigned int address_v);
/* Source D3D9 raster states (verified material bridge only):
   cull_mode: 0=NONE, 1=cull CW, 2=cull CCW (per-eye screen pixels).
   depth_mode: 0=LESS, 1=LESS_EQUAL. depth_write: 0/1.
   These arguments are only trusted when a separately authenticated
   gameplay world pass is active; no automatic D3D9 Draw hooking. */
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithRenderStates(
    const float* view_space_xyz, const float* interleaved_uv,
    unsigned int vertex_count,
    const std::uint32_t* triangle_indices, unsigned int index_count,
    const unsigned char* texture_rgba,
    unsigned int texture_width, unsigned int texture_height,
    unsigned int texture_pitch, unsigned int alpha_cutoff,
    unsigned int filter_mode, unsigned int address_u, unsigned int address_v,
    unsigned int cull_mode, unsigned int depth_mode,
    unsigned int depth_write);


// Reads *actual* fixed-function D3D9 UP XYZ+TEX1 source vertices,
// original D3DTS_WORLD/VIEW transforms and stage-0 CPU-lockable texture.
// It is callable only during a proven BeginVerifiedWorldFrame transaction.
// Unknown shader/material states and GPU-only textures fail closed.
// vertex_data must be a full, tightly-strided vertex list. Indices are
// triangle-list values relative to that list (not DrawIndexedUP MinIndex).
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9UPTextured(
    void* d3d9_device, const void* vertex_data,
    unsigned int vertex_count, unsigned int stride_bytes,
    const std::uint32_t* triangle_indices, unsigned int index_count,
    unsigned int alpha_cutoff);
// Source-backed VB/IB companions for a trusted world-pass adapter.
// Both accept only actual D3DPT_TRIANGLELIST source calls and use CPU-
// readable, non-WRITEONLY stream-0 FVF XYZ/TEX1 buffers. They do not
// independently assert the verified-world proof or hook GPU draws.
// Mirrors DrawPrimitive(StartVertex, PrimitiveCount).
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9VBTextured(
    void* d3d9_device, unsigned int start_vertex,
    unsigned int primitive_count, unsigned int alpha_cutoff);
// Mirrors DrawIndexedPrimitive(BaseVertexIndex, MinVertexIndex,
// NumVertices, StartIndex, PrimitiveCount).
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedVBTextured(
    void* d3d9_device, int base_vertex_index,
    unsigned int min_vertex_index, unsigned int num_vertices,
    unsigned int start_index, unsigned int primitive_count,
    unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_EndVerifiedWorldFrame();
__declspec(dllexport) void __stdcall Model2VR_AbortVerifiedWorldFrame();
}

// Optional explicit recenter on a proven scene/device restart.
// Returns FALSE if the renderer currently owns an active world frame.
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_ResetVerifiedWorldTrackingOrigin();
