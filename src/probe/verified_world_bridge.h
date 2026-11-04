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
   0 for CLAMP, 1 for WRAP, 2 for MIRROR, 3 for MIRRORONCE.
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
   depth_mode: 0=LESS, 1=LESS_EQUAL, 2=ALWAYS (legacy),
               3=EQUAL, 4=GREATER, 5=GREATER_EQUAL, 6=NOT_EQUAL.
   depth_write: 0/1. NEVER and unknown comparisons fail closed.
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


// Optional verified D3D9 diffuse RGBA (source D3DCOLOR A8R8G8B8),
// perspective-correct interpolated after independent-eye clipping.
// Both MODULATE flags are boolean and require a non-null per-vertex
// array. SELECTARG1(TEXTURE) preserves the old ABI and material semantics.
// Never call outside a separately authenticated original world frame.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedTexturedTrianglesWithDiffuseRenderStates(
    const float* view_space_xyz,const float* interleaved_uv,
    unsigned int vertex_count,const std::uint32_t* triangle_indices,
    unsigned int index_count,const unsigned char* texture_rgba,
    unsigned int texture_width,unsigned int texture_height,
    unsigned int texture_pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int modulate_rgb,
    unsigned int modulate_alpha);

// Optional original D3D9 non-separate alpha blend state, world-only:
// 0 opaque, 1 SRCALPHA/INVSRCALPHA, 2 ONE/ONE, 3 SRCALPHA/ONE,
// 4 ONE/INVSRCALPHA, 5 ONE/ZERO (preserves source alpha).
// D3DBLENDOP_ADD only; unknown states are rejected
// before this entrypoint. Legacy DiffuseRenderStates ABI is unchanged.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedTexturedTrianglesWithDiffuseBlendRenderStates(
    const float* view_space_xyz,const float* interleaved_uv,
    unsigned int vertex_count,const std::uint32_t* triangle_indices,
    unsigned int index_count,const unsigned char* texture_rgba,
    unsigned int texture_width,unsigned int texture_height,
    unsigned int texture_pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int modulate_rgb,
    unsigned int modulate_alpha,unsigned int blend_mode);

// Extended independent D3D9 stage-zero RGB/ALPHA channel selectors:
// 0=TEXTURE, 1=DIFFUSE, 2=MODULATE(TEXTURE,DIFFUSE).
// A SELECTARG(DIFFUSE) channel requires exact source vertex D3DCOLOR
// (not an inferred color); both eyes use identical source operations.
// This does not grant gameplay-world ownership. Existing exports retain
// their ABI; unknown selectors reject and abort the in-flight world frame.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedTexturedTrianglesWithMaterialSourceRenderStates(
    const float* view_space_xyz,const float* interleaved_uv,
    unsigned int vertex_count,const std::uint32_t* triangle_indices,
    unsigned int index_count,const unsigned char* texture_rgba,
    unsigned int texture_width,unsigned int texture_height,
    unsigned int texture_pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode);

// Source D3D9 COLORWRITEENABLE0 byte mask (RGBA bits 1,2,4,8).
// With rgb_source=alpha_source=1 (DIFFUSE/CURRENT), NULL texture plus
// zero width, height and pitch is legal. A texture-dependent source still
// requires the original nonempty RGBA bytes. Depth mode 2 is ALWAYS.
// Old material export remains ABI-compatible with the full 0xF mask.
// Zero RGB/A mask can still produce Z, but NOT stereo eye-color coverage.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskRenderStates(
    const float* view_space_xyz,const float* interleaved_uv,
    unsigned int vertex_count,const std::uint32_t* triangle_indices,
    unsigned int index_count,const unsigned char* texture_rgba,
    unsigned int texture_width,unsigned int texture_height,
    unsigned int texture_pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode,
    unsigned int write_rgba_mask);

// Exact original D3D9 alpha comparison for an explicitly authenticated
// original source Draw. mode: 0=legacy byte >= alpha_cutoff; 1..8 match
// D3DCMP_NEVER, LESS, EQUAL, LESSEQUAL, GREATER, NOTEQUAL,
// GREATEREQUAL, ALWAYS. Exact modes require alpha_cutoff=0 and
// alpha_reference=0..255; legacy mode requires alpha_reference=0.
// The existing ColorMaskRenderStates export and binary ABI are unchanged.
// This entrypoint never authorizes an unknown world/HUD Draw by itself.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskAlphaCompareRenderStates(
    const float* view_space_xyz,const float* interleaved_uv,
    unsigned int vertex_count,const std::uint32_t* triangle_indices,
    unsigned int index_count,const unsigned char* texture_rgba,
    unsigned int texture_width,unsigned int texture_height,
    unsigned int texture_pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode,
    unsigned int write_rgba_mask,unsigned int alpha_compare,
    unsigned int alpha_reference);

// Actual x86 D3D9 SEPARATEALPHABLENDENABLE with independent ADD alpha
// factors is supported only by this additional opt-in verified-world ABI.
// Separate factor codes: 0 ZERO, 1 ONE, 2 SRCALPHA/SRCCOLOR,
// 3 INVSRCALPHA/INVSRCCOLOR, 4 DESTALPHA/DESTCOLOR,
// 5 INVDESTALPHA/INVDESTCOLOR. When separate_alpha_enabled=0 the
// canonical unused factors MUST be 1/0. Unsupported D3D9 source factors,
// operations and world owners reject before either OpenXR eye is written.
// The old MaterialColorMaskAlphaCompareRenderStates export is unchanged.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates(
    const float* view_space_xyz,const float* interleaved_uv,
    unsigned int vertex_count,const std::uint32_t* triangle_indices,
    unsigned int index_count,const unsigned char* texture_rgba,
    unsigned int texture_width,unsigned int texture_height,
    unsigned int texture_pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode,
    unsigned int write_rgba_mask,unsigned int alpha_compare,
    unsigned int alpha_reference,unsigned int separate_alpha_enabled,
    unsigned int separate_alpha_src,unsigned int separate_alpha_dst);

// Return TRUE only during an independently authorized current-thread world
// transaction. Adapters must fail before reading original D3D9 draw memory.
__declspec(dllexport) BOOL __stdcall Model2VR_VerifiedWorldFrameOwnedByCurrentThread();
// Explicit one-Draw opt-in for an independently authenticated native
// Daytona race World3D adapter. Arm only AFTER BeginVerifiedWorldFrame
// succeeded and from the same render thread, using the physical upcoming
// Present, next real all-family Draw ordinal, ORIGINAL game EXE Draw
// caller RVA, family 1..4 and successful source BeginScene ordinal from
// independent private evidence. This excludes same-caller HUD/mirror draws.
// The original D3D9 hook consumes the rule exactly once and forwards the
// actual UP/VB/IB source arguments into the already calibrated CPU two-eye
// material raster before executing the genuine COM Draw ONCE.
// cpu_projector_rva=0 uses original fixed XYZ+VIEW/PROJ; nonzero uses
// separately authenticated CPU ProjectVertexToScreen source XYZRHW.
// Alpha cutoff 256 means exact original D3D9 alpha test, 0..255 legacy.
// Missing/mismatched provenance never auto-authorizes a HUD/sky/aux Draw.
// An unused armed rule makes End reject the entire incomplete binocular
// source frame; original game and mono backup are unaffected.
__declspec(dllexport) BOOL __stdcall Model2VR_ArmVerifiedOriginalD3D9Draw(
    unsigned long long original_present_frame,
    unsigned long long original_draw_ordinal,unsigned int original_family,
    unsigned int original_caller_rva,unsigned int original_scene_ordinal,
    unsigned int authenticated_cpu_projector_caller_rva,
    unsigned int alpha_cutoff);
// Read-only physical source phase. Query on the render thread immediately
// BEFORE a genuine Draw, then arm its exact all-family ordinal and scene.
// Neither value is gameplay World3D proof. Invalid ownership returns zero.
__declspec(dllexport) unsigned long long __stdcall
Model2VR_NextOriginalD3D9DrawOrdinal();
__declspec(dllexport) unsigned int __stdcall
Model2VR_CurrentOriginalD3D9SceneOrdinal();
// The exact D3D9 COM device whose original Draw/Present epochs are hooked.
// Secondary devices, overlays and swap-in replacements must never masquerade
// as this emulator's world output. No refs acquired and no state changed.
__declspec(dllexport) BOOL __stdcall Model2VR_IsOriginalPrimaryD3D9Device(
    const void* d3d9_device);
// Reject CPU projected-screen matches if the ORIGINAL source matrix or
// geometry command has changed between CPU projection and GPU Draw.
// This comparison grants NO world-pass proof on its own.
__declspec(dllexport) BOOL __stdcall Model2VR_OriginalCpuProjectionStillMatchesDraw(
    unsigned int original_matrix_identity,
    unsigned int original_matrix_content_fingerprint,
    unsigned int original_geometry_command_word);
// The original projected XYZRHW/TEX1 D3D9 UP geometry can be converted to
// calibrated 6DoF eyes ONLY if a separately proven world transaction is
// active, at least two same-thread source triangles match the CPU projector
// in the current original Present epoch, source caller RVA is authenticated,
// the screen projection is consistent and actual fixed-function sampler/
// raster/texture states pass the existing strict material gate.
// No implicit hook or unknown shader-UP source is promoted into world.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedProjectedD3D9UPTextured(
    void* d3d9_device,const void* original_projected_vertices,
    unsigned int original_triangle_count,unsigned int stride_bytes,
    unsigned int authenticated_cpu_project_caller_rva,
    unsigned int alpha_cutoff);
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

// Original fixed-function XYZRHW/TEX1 STREAM-0 equivalents of the trusted
// UP CPU-projector bridge. These APIs only expand a bounded CPU-lockable VB
// (and 16/32-bit IB) into the ACTUAL source triangle order, then reuse the
// same-thread CPU projector proof, perspective fit, D3D9 material gate and
// calibrated two-eye raster. Both require active explicit world ownership,
// a separately authenticated CPU gameplay projector caller RVA and a
// TRIANGLELIST draw of 2..8 triangles. They do not auto-hook or classify HUD.
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9VBTextured(
    void* d3d9_device,unsigned int start_vertex,
    unsigned int primitive_count,
    unsigned int authenticated_cpu_project_caller_rva,
    unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9IndexedVBTextured(
    void* d3d9_device,int base_vertex_index,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int start_index,unsigned int primitive_count,
    unsigned int authenticated_cpu_project_caller_rva,
    unsigned int alpha_cutoff);
// Original VB/IB DrawPrimitive and DrawIndexedPrimitive list/strip/fan
// variants retain source winding and lock only referenced original vertices.
// The original list-only exports above remain ABI compatible. No automatic
// world classification, original Draw replay, or additional FFB tick.
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9VBTopologyTextured(
    void* device,unsigned int original_topology,unsigned int start_vertex,
    unsigned int primitive_count,unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedVBTopologyTextured(
    void* device,unsigned int original_topology,int base_vertex_index,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int start_index,unsigned int primitive_count,
    unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9VBTopologyTextured(
    void* device,unsigned int original_topology,unsigned int start_vertex,
    unsigned int primitive_count,unsigned int authenticated_cpu_caller_rva,
    unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9IndexedVBTopologyTextured(
    void* device,unsigned int original_topology,int base_vertex_index,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int start_index,unsigned int primitive_count,
    unsigned int authenticated_cpu_caller_rva,unsigned int alpha_cutoff);

// Original DrawIndexedPrimitiveUP's source pointer begins at MinVertexIndex.
// Both APIs require a separately proven frame and are NEVER generic hooks;
// source is copied, 16/32-bit indices normalized, and unknown HUD refused.
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedUPTextured(
    void* d3d9_device,unsigned int min_vertex_index,
    unsigned int num_vertices,unsigned int primitive_count,
    const void* original_indices,unsigned int index_format,
    const void* original_vertices,unsigned int stride_bytes,
    unsigned int alpha_cutoff);
// For XYZRHW/TEX1, additionally require 2..8 unambiguous original CPU
// projected triangles, same render thread/Present and authenticated caller.
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedProjectedD3D9IndexedUPTextured(
    void* d3d9_device,unsigned int min_vertex_index,
    unsigned int num_vertices,unsigned int primitive_count,
    const void* original_indices,unsigned int index_format,
    const void* original_vertices,unsigned int stride_bytes,
    unsigned int authenticated_cpu_project_caller_rva,
    unsigned int alpha_cutoff);
// Original DrawPrimitiveUP can carry TRIANGLELIST, TRIANGLESTRIP or
// TRIANGLEFAN; convert winding/source order only inside an independently
// authenticated world frame. Original D3D9 draw and fallback are untouched.
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9PrimitiveUPTopologyTextured(
    void* d3d9_device,unsigned int original_primitive_type,
    unsigned int primitive_count,const void* original_vertices,
    unsigned int stride_bytes,unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedProjectedD3D9PrimitiveUPTopologyTextured(
    void* d3d9_device,unsigned int original_primitive_type,
    unsigned int primitive_count,const void* original_vertices,
    unsigned int stride_bytes,unsigned int authenticated_cpu_project_caller_rva,
    unsigned int alpha_cutoff);
// Original DrawIndexedPrimitiveUP TRIANGLESTRIP/FAN/LIST route. Strictly
// explicit world-only source adapters; existing TRIANGLELIST ABI retained.
__declspec(dllexport) BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedUPTopologyTextured(
    void* d3d9_device,unsigned int original_primitive_type,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int primitive_count,const void* original_indices,
    unsigned int index_format,const void* original_vertices,
    unsigned int stride_bytes,unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall
Model2VR_AppendVerifiedProjectedD3D9IndexedUPTopologyTextured(
    void* d3d9_device,unsigned int original_primitive_type,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int primitive_count,const void* original_indices,
    unsigned int index_format,const void* original_vertices,
    unsigned int stride_bytes,unsigned int authenticated_cpu_project_caller_rva,
    unsigned int alpha_cutoff);
__declspec(dllexport) BOOL __stdcall Model2VR_EndVerifiedWorldFrame();
// Source-owned upcoming original D3D9 Present frame ID (nonzero only
// after real device hooks are installed). Available in GPU-replay mode too;
// an ID never grants gameplay-world permission. Query before original Present.
__declspec(dllexport) unsigned long long __stdcall
Model2VR_NextOriginalD3D9PresentFrameId();
// Preferred opt-in source-connected end: ARM, but never publish before the
// matching ORIGINAL D3D9 Present HRESULT succeeds. The emulator adapter
// must supply the exact upcoming original frame ID to BeginVerifiedWorldFrame.
// An authenticated world pair takes precedence over experimental GPU
// replay only at its matching successful original Present. Failed/unknown
// sources fall back without disabling the original game, HUD or FFB.
__declspec(dllexport) BOOL __stdcall Model2VR_EndVerifiedWorldFrameOnOriginalPresent();
// Read-only exact pending world handoff; no world source classification.
__declspec(dllexport) BOOL __stdcall
Model2VR_HasVerifiedWorldPendingForOriginalPresent(unsigned long long original_frame);
__declspec(dllexport) void __stdcall Model2VR_AbortVerifiedWorldFrame();
// When ANY explicitly trusted original D3D9 source draw cannot be
// reproduced, revoke its entire partial CPU-world frame. The authentic
// original game Draw, source Present, and GPU/mono fallback are untouched.
__declspec(dllexport) void __stdcall Model2VR_RejectVerifiedD3D9SourceDraw(
    unsigned int original_source_kind);
}

// Optional explicit recenter on a proven scene/device restart.
// Returns FALSE if the renderer currently owns an active world frame.
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_ResetVerifiedWorldTrackingOrigin();
