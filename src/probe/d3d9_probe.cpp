#include "logger.h"
#include "draw_up_witness.hpp"
#include "daytona_native_stereo.hpp"
#include "daytona_up_decl_witness.hpp"
#include "draw_origin_gate.hpp"
#include "project_draw_correlator.hpp"
#include "d3d9_source_pass_context.hpp"
#include "d3d9_sampler_states.hpp"
#include "model2_camera_projection_fit.hpp"
#include "d3d9_primary_device_owner.hpp"
#include "verified_world_present_gate.hpp"
#include "original_d3d9_draw_result_gate.hpp"
#include "original_d3d9_clear_result_gate.hpp"
#include "original_d3d9_surface_result_gate.hpp"
#include "original_d3d9_lineage.hpp"
#include "original_d3d9_control_present.hpp"
#include "original_d3d9_scene_draw_coverage.hpp"
#include "original_d3d9_draw_target_coverage.hpp"
#include "original_d3d9_camera_com_lifetime.hpp"
#include "original_d3d9_caller_bloom.hpp"
#include "original_d3d9_caller_inventory.hpp"
#include "original_d3d9_validation_policy.hpp"
#include "original_d3d9_resource_owner.hpp"
#include "original_d3d9_transform_lineage.hpp"
#include "original_d3d9_stream_frequency.hpp"
#include "original_d3d9_raster_mode.hpp"
#include "original_d3d9_unmirrored_geometry.hpp"
#include "original_d3d9_buffer_mutation.hpp"
#include "original_d3d9_authenticated_draw.hpp"
#include "verified_world_bridge.h"
#include "d3d9_world_source.hpp"
#include "d3d9_projected_vb_observer.hpp"
#include "d3d9_indexed_up_window.hpp"
#include "d3d9_primitive_topology.hpp"
#include "projected_up_world_source.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <d3d9.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

// Passive Direct3D9 command-boundary instrumentation. This deliberately DOES
// NOT duplicate draw commands or change render targets. All per-eye replay
// remains disabled until the exact CPU world-pass and GPU state agree.
// API discovery is based on documented COM method order, not static addresses.
namespace {
using Create9Fn=IDirect3D9* (WINAPI*)(UINT);
using CreateDeviceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
using CreateAdditionalSwapChainFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**);
using GetSwapChainFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,IDirect3DSwapChain9**);
using OriginalSwapChainPresentFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
using OriginalSwapChainGetFrontBufferDataFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DSwapChain9*,IDirect3DSurface9*);
using OriginalSwapChainGetBackBufferFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DSwapChain9*,UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9**);
using CreateQueryFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DQUERYTYPE,IDirect3DQuery9**);
using OriginalQueryIssueFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DQuery9*,DWORD);
using OriginalQueryGetDataFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DQuery9*,void*,DWORD,DWORD);
using SetCursorPropertiesFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,IDirect3DSurface9*);
using SetCursorPositionFn=void (STDMETHODCALLTYPE*)(IDirect3DDevice9*,int,int,DWORD);
using ShowCursorFn=BOOL (STDMETHODCALLTYPE*)(IDirect3DDevice9*,BOOL);
using ValidateDeviceFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT*);
using EvictManagedResourcesFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using SetDialogBoxModeFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,BOOL);
using SetGammaRampFn=void (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,DWORD,const D3DGAMMARAMP*);
using PresentFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,CONST RECT*,CONST RECT*,HWND,CONST RGNDATA*);
using ResetFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using SceneFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using CreateVertexBufferFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9**,HANDLE*);
using CreateIndexBufferFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9**,HANDLE*);
using CreateTextureFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,
    IDirect3DTexture9**,HANDLE*);
using CreateVolumeTextureFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,
    IDirect3DVolumeTexture9**,HANDLE*);
using CreateCubeTextureFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,
    IDirect3DCubeTexture9**,HANDLE*);
using CreateRenderTargetFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,D3DFORMAT,D3DMULTISAMPLE_TYPE,DWORD,BOOL,
    IDirect3DSurface9**,HANDLE*);
using CreateDepthStencilSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,D3DFORMAT,D3DMULTISAMPLE_TYPE,DWORD,BOOL,
    IDirect3DSurface9**,HANDLE*);
using CreateOffscreenPlainSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,D3DFORMAT,D3DPOOL,IDirect3DSurface9**,HANDLE*);
using GetBackBufferFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9**);
using GetRenderTargetFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,DWORD,IDirect3DSurface9**);
using GetDepthStencilSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9**);
using GetRenderTargetDataFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,IDirect3DSurface9*);
using GetFrontBufferDataFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,IDirect3DSurface9*);
using UpdateSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const POINT*);
using UpdateTextureFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DBaseTexture9*,IDirect3DBaseTexture9*);
// Genuine device-wide palette changes can alter bound paletted texels.
using SetPaletteEntriesFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const PALETTEENTRY*);
using SetCurrentTexturePaletteFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT);
using StretchRectFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,
    IDirect3DSurface9*,const RECT*,D3DTEXTUREFILTERTYPE);
using ColorFillFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,D3DCOLOR);
using SetRenderTargetFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,DWORD,IDirect3DSurface9*);
using SetDepthStencilSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*);
using SetTransformFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
using MultiplyTransformFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
using SetVSConstantFFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,const float*,UINT);
using SetViewportFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,const D3DVIEWPORT9*);
using SetScissorFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*);
using SetStreamSourceFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT stream,IDirect3DVertexBuffer9* buffer,UINT offset,UINT stride);
using GetStreamSourceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,UINT,IDirect3DVertexBuffer9**,UINT*,UINT*);
using BufferLockFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DResource9*,UINT,UINT,void**,DWORD);
using BufferUnlockFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DResource9*);
using SetIndicesFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DIndexBuffer9* buffer);
using GetIndicesFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DIndexBuffer9**);
using SetVertexDeclarationFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DVertexDeclaration9* declaration);
using SetFVFFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD fvf);
using SetVertexShaderFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DVertexShader9* shader);
using SetPixelShaderFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DPixelShader9* shader);
using SetPSConstantFFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const float*,UINT);
using SetTextureFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,IDirect3DBaseTexture9*);
using GetTextureFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,DWORD,IDirect3DBaseTexture9**);
using TextureLockRectFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DTexture9*,UINT,D3DLOCKED_RECT*,const RECT*,DWORD);
using TextureUnlockRectFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DTexture9*,UINT);
using TextureGetSurfaceLevelFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DTexture9*,UINT,IDirect3DSurface9**);
using SurfaceGetContainerFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DSurface9*,REFIID,void**);
using VolumeGetContainerFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DVolume9*,REFIID,void**);
using SurfaceLockRectFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DSurface9*,D3DLOCKED_RECT*,const RECT*,DWORD);
using SurfaceUnlockRectFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DSurface9*);
using SurfaceGetDCFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DSurface9*,HDC*);
using SurfaceReleaseDCFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DSurface9*,HDC);
using MipLodFn=DWORD (STDMETHODCALLTYPE*)(IDirect3DBaseTexture9*,DWORD);
using MipFilterFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DBaseTexture9*,D3DTEXTUREFILTERTYPE);
using MipGenerateFn=void (STDMETHODCALLTYPE*)(IDirect3DBaseTexture9*);
using TextureDirtyFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DTexture9*,const RECT*);
using CubeDirtyFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DCubeTexture9*,D3DCUBEMAP_FACES,const RECT*);
using VolumeDirtyFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DVolumeTexture9*,const D3DBOX*);
using CubeGetSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DCubeTexture9*,D3DCUBEMAP_FACES,UINT,IDirect3DSurface9**);
using VolumeGetLevelFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DVolumeTexture9*,UINT,IDirect3DVolume9**);
using VolumeLevelLockFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DVolume9*,D3DLOCKED_BOX*,const D3DBOX*,DWORD);
using VolumeLevelUnlockFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DVolume9*);
using CubeLockRectFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DCubeTexture9*,D3DCUBEMAP_FACES,UINT,D3DLOCKED_RECT*,const RECT*,DWORD);
using CubeUnlockRectFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DCubeTexture9*,D3DCUBEMAP_FACES,UINT);
using VolumeLockBoxFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DVolumeTexture9*,UINT,D3DLOCKED_BOX*,const D3DBOX*,DWORD);
using VolumeUnlockBoxFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DVolumeTexture9*,UINT);
using SetTextureStageStateFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,D3DTEXTURESTAGESTATETYPE,DWORD);
using SetSamplerStateFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,D3DSAMPLERSTATETYPE,DWORD);
using SetRenderStateFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DRENDERSTATETYPE,DWORD);
using SetShaderIntFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const int*,UINT);
using SetShaderBoolFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const BOOL*,UINT);
using SetFrequencyFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT);
// Track authentic source fixed-function state without inspecting game data.
using SetMaterialFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,const D3DMATERIAL9*);
using SetLightFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,const D3DLIGHT9*);
using LightEnableFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,BOOL);
using SetClipPlaneFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,const float*);
using SetClipStatusFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,const D3DCLIPSTATUS9*);
using SetSoftwareVPFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,BOOL);
using SetNPatchFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,float);
// Authentically processed vertex buffers and higher-order patch draws.
using ProcessVerticesFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,IDirect3DVertexBuffer9*,IDirect3DVertexDeclaration9*,DWORD);
using DrawRectPatchFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const float*,const D3DRECTPATCH_INFO*);
using DrawTriPatchFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const float*,const D3DTRIPATCH_INFO*);
using DeletePatchFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT);
// IDirect3DDevice9 COM 59/60/61 and IDirect3DStateBlock9::Apply slot 5.
// Block Apply may restore unobserved camera/material values without Set* COM.
using CreateStateBlockFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DSTATEBLOCKTYPE,IDirect3DStateBlock9**);
using BeginStateBlockFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*);
using EndStateBlockFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DStateBlock9**);
using StateBlockApplyFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
using ClearFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD);
using PrimitiveFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
using IndexedFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
using PrimitiveUPFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
using IndexedUPFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);

Create9Fn g_create9=nullptr;
CreateDeviceFn g_create_device=nullptr;
PresentFn g_present=nullptr;
SetCursorPropertiesFn g_original_set_cursor_properties=nullptr;
SetCursorPositionFn g_original_set_cursor_position=nullptr;
ShowCursorFn g_original_show_cursor=nullptr;
std::atomic<bool> g_original_cursor_visibility_known{false};
std::atomic<bool> g_original_cursor_visible{false};
std::atomic<bool> g_original_cursor_unknown{false};
std::array<std::atomic<std::uint32_t>,3> g_original_cursor_controls{};
SetDialogBoxModeFn g_original_set_dialog_box_mode=nullptr;
SetGammaRampFn g_original_set_gamma_ramp=nullptr;
std::atomic<bool> g_original_dialog_box_active{false};
std::atomic<bool> g_original_gamma_unproven{false};
std::atomic<bool> g_original_external_presentation_unproven{false};
std::array<std::atomic<std::uint32_t>,2> g_original_presentation_controls{};
ValidateDeviceFn g_validate_device=nullptr;
EvictManagedResourcesFn g_evict_managed_resources=nullptr;
std::atomic<bool> g_residency_foreign_unproven{false};
std::atomic<std::uint64_t> g_residency_rejected_present{0u};
std::atomic<bool> g_original_validation_unproven{false};
std::atomic<bool> g_original_validation_foreign_caller{false};
std::atomic<std::uint64_t> g_original_validation_rejected_present{0u};
std::array<std::atomic<std::uint32_t>,4> g_original_validation_counts{};
CreateQueryFn g_create_query=nullptr;
OriginalQueryIssueFn g_original_query_issue=nullptr;
OriginalQueryGetDataFn g_original_query_get_data=nullptr;
void* g_original_query_issue_target=nullptr;
void* g_original_query_get_data_target=nullptr;
std::mutex g_original_query_hook_mutex;
std::atomic<bool> g_original_query_source_unproven{false};
std::atomic<std::uint64_t> g_original_query_rejected_present{0u};
std::array<std::atomic<std::uint32_t>,4> g_original_query_calls{};
CreateAdditionalSwapChainFn g_create_additional_swap_chain=nullptr;
GetSwapChainFn g_get_swap_chain=nullptr;
OriginalSwapChainPresentFn g_original_swap_chain_present=nullptr;
OriginalSwapChainGetBackBufferFn g_original_swap_chain_get_back_buffer=nullptr;
OriginalSwapChainGetFrontBufferDataFn g_original_swap_chain_get_front_buffer_data=nullptr;
void* g_original_swap_chain_present_target=nullptr;
void* g_original_swap_chain_get_back_target=nullptr;
void* g_original_swap_chain_get_front_target=nullptr;
std::mutex g_original_swap_chain_hook_mutex;
// An additional physical SwapChain Present cannot be assigned the device
// Present's Draw/camera/source epoch. Remain mono-safe for this process.
std::atomic<bool> g_original_swap_chain_source_unproven{false};
std::array<std::atomic<std::uint32_t>,4> g_original_swap_chain_calls{};
std::atomic<std::uint32_t> g_original_swap_chain_readbacks{0u};
ResetFn g_reset=nullptr;
SceneFn g_begin_scene=nullptr;
SceneFn g_end_scene=nullptr;
ClearFn g_clear=nullptr;
CreateVertexBufferFn g_create_vertex_buffer=nullptr;
CreateIndexBufferFn g_create_index_buffer=nullptr;
std::array<std::atomic<std::uint32_t>,2> g_original_buffer_creates{};
CreateTextureFn g_create_texture=nullptr;
CreateVolumeTextureFn g_create_volume_texture=nullptr;
CreateCubeTextureFn g_create_cube_texture=nullptr;
std::array<std::atomic<std::uint32_t>,3> g_original_texture_creates{};
CreateRenderTargetFn g_create_render_target=nullptr;
CreateDepthStencilSurfaceFn g_create_depth_stencil_surface=nullptr;
CreateOffscreenPlainSurfaceFn g_create_offscreen_plain_surface=nullptr;
std::array<std::atomic<std::uint32_t>,3> g_original_surface_creates{};
GetBackBufferFn g_get_back_buffer=nullptr;
GetRenderTargetFn g_get_render_target=nullptr;
GetDepthStencilSurfaceFn g_get_depth_stencil_surface=nullptr;
std::array<std::atomic<std::uint32_t>,3> g_original_surface_getters{};
GetRenderTargetDataFn g_get_render_target_data=nullptr;
GetFrontBufferDataFn g_get_front_buffer_data=nullptr;
std::array<std::atomic<std::uint32_t>,2> g_original_surface_readbacks{};
// Original content writes by *physical* game D3D9 GPU methods.
// Counts are anonymous/per-Present: Clear, UpdateSurface, StretchRect, ColorFill.
std::array<std::atomic<std::uint32_t>,4> g_original_gpu_surface_writes{};
// Three read-only binding-return aliases; never leak resource identities.
std::array<std::atomic<std::uint32_t>,3> g_original_resource_getters{};
UpdateSurfaceFn g_update_surface=nullptr;
UpdateTextureFn g_update_texture=nullptr;
SetPaletteEntriesFn g_set_palette_entries=nullptr;
SetCurrentTexturePaletteFn g_set_current_texture_palette=nullptr;
StretchRectFn g_stretch_rect=nullptr;
ColorFillFn g_color_fill=nullptr;
SetRenderTargetFn g_set_render_target=nullptr;
SetDepthStencilSurfaceFn g_set_depth_surface=nullptr;
SetTransformFn g_set_transform=nullptr;
MultiplyTransformFn g_multiply_transform=nullptr;
SetVSConstantFFn g_set_vs_constant_f=nullptr;
SetViewportFn g_set_viewport=nullptr;
SetScissorFn g_set_scissor=nullptr;
SetStreamSourceFn g_set_stream_source=nullptr;
GetStreamSourceFn g_get_stream_source=nullptr;
BufferLockFn g_vb_lock=nullptr,g_ib_lock=nullptr;
BufferUnlockFn g_vb_unlock=nullptr,g_ib_unlock=nullptr;
void* g_vb_lock_target=nullptr;
void* g_ib_lock_target=nullptr;
void* g_vb_unlock_target=nullptr;
void* g_ib_unlock_target=nullptr;
std::mutex g_buffer_hook_mutex;
std::atomic<bool> g_buffer_hook_unknown{false};
m2vr::drawprobe::OriginalBoundWriteLocks g_original_bound_write_locks{};
std::array<std::atomic<std::uint32_t>,2> g_original_buffer_writes{};
SetIndicesFn g_set_indices=nullptr;
GetIndicesFn g_get_indices=nullptr;
SetVertexDeclarationFn g_set_vertex_declaration=nullptr;
SetFVFFn g_set_f_v_f=nullptr;
SetVertexShaderFn g_set_vertex_shader=nullptr;
SetPixelShaderFn g_set_pixel_shader=nullptr;
SetPSConstantFFn g_set_ps_constant_f=nullptr;
SetTextureFn g_set_texture=nullptr;
GetTextureFn g_get_texture=nullptr;
TextureLockRectFn g_texture_lock_rect=nullptr;
TextureUnlockRectFn g_texture_unlock_rect=nullptr;
TextureGetSurfaceLevelFn g_texture_get_surface_level=nullptr;
SurfaceGetContainerFn g_surface_get_container=nullptr;
VolumeGetContainerFn g_volume_get_container=nullptr;
SurfaceLockRectFn g_surface_lock_rect=nullptr;
SurfaceUnlockRectFn g_surface_unlock_rect=nullptr;
SurfaceGetDCFn g_surface_get_dc=nullptr;
SurfaceReleaseDCFn g_surface_release_dc=nullptr;
void* g_texture_get_surface_level_target=nullptr;
void* g_surface_container_target=nullptr;
void* g_surface_lock_target=nullptr;
void* g_surface_unlock_target=nullptr;
void* g_surface_get_dc_target=nullptr;
void* g_surface_release_dc_target=nullptr;
std::mutex g_texture_surface_hook_mutex;
std::atomic<std::uint32_t> g_original_texture_surface_writes{0u};
std::array<MipLodFn,3> g_mip_lod{};
std::array<void*,3> g_mip_lod_target{};
std::array<MipFilterFn,3> g_mip_filter{};
std::array<MipGenerateFn,3> g_mip_generate{};
std::array<void*,3> g_mip_filter_target{};
std::array<void*,3> g_mip_generate_target{};
std::array<void*,3> g_dirty_target{};
TextureDirtyFn g_texture_dirty=nullptr;
CubeDirtyFn g_cube_dirty=nullptr;
VolumeDirtyFn g_volume_dirty=nullptr;
std::mutex g_mip_mutex;
CubeGetSurfaceFn g_cube_get_surface=nullptr;
VolumeGetLevelFn g_volume_get_level=nullptr;
VolumeLevelLockFn g_volume_level_lock=nullptr;
VolumeLevelUnlockFn g_volume_level_unlock=nullptr;
void* g_cube_get_surface_target=nullptr;
void* g_volume_get_level_target=nullptr;
void* g_volume_container_target=nullptr;
void* g_volume_level_lock_target=nullptr;
void* g_volume_level_unlock_target=nullptr;
std::mutex g_volume_level_hook_mutex;
CubeLockRectFn g_cube_lock_rect=nullptr;
CubeUnlockRectFn g_cube_unlock_rect=nullptr;
VolumeLockBoxFn g_volume_lock_box=nullptr;
VolumeUnlockBoxFn g_volume_unlock_box=nullptr;
void* g_cube_lock_target=nullptr;
void* g_cube_unlock_target=nullptr;
void* g_volume_lock_target=nullptr;
void* g_volume_unlock_target=nullptr;
std::array<std::atomic<std::uint32_t>,2> g_original_texture_shape_writes{};
void* g_texture_lock_target=nullptr;
void* g_texture_unlock_target=nullptr;
std::mutex g_texture_hook_mutex;
std::atomic<bool> g_texture_hook_unknown{false};
m2vr::drawprobe::OriginalBoundWriteLocks g_original_texture_write_locks{};
std::atomic<std::uint32_t> g_original_texture_lock_writes{0u};
SetTextureStageStateFn g_set_texture_stage=nullptr;
SetSamplerStateFn g_set_sampler_state=nullptr;
SetRenderStateFn g_set_render_state=nullptr;
SetShaderIntFn g_vs_i=nullptr;
SetShaderBoolFn g_vs_b=nullptr;
SetShaderIntFn g_ps_i=nullptr;
SetShaderBoolFn g_ps_b=nullptr;
SetFrequencyFn g_freq=nullptr;
SetMaterialFn g_set_material=nullptr;
SetLightFn g_set_light=nullptr;
LightEnableFn g_light_enable=nullptr;
SetClipPlaneFn g_set_clip_plane=nullptr;
SetClipStatusFn g_set_clip_status=nullptr;
SetSoftwareVPFn g_set_software_vp=nullptr;
SetNPatchFn g_set_npatch_mode=nullptr;
ProcessVerticesFn g_process_vertices=nullptr;
DrawRectPatchFn g_draw_rect_patch=nullptr;
DrawTriPatchFn g_draw_tri_patch=nullptr;
DeletePatchFn g_delete_patch=nullptr;
CreateStateBlockFn g_create_state_block=nullptr;
BeginStateBlockFn g_begin_state_block=nullptr;
EndStateBlockFn g_end_state_block=nullptr;
StateBlockApplyFn g_state_block_apply=nullptr;
void* g_state_block_apply_target=nullptr;
std::mutex g_state_block_hook_mutex;
std::atomic<bool> g_state_block_apply_unobserved{false};
m2vr::drawprobe::OriginalStreamFrequencyState g_original_stream_frequency_state{};
m2vr::drawprobe::OriginalRasterModeState g_original_raster_mode_state{};
m2vr::drawprobe::OriginalUnmirroredGeometryState g_unmirrored_geometry_state{};
PrimitiveFn g_draw=nullptr;
IndexedFn g_draw_indexed=nullptr;
PrimitiveUPFn g_draw_up=nullptr;
IndexedUPFn g_draw_indexed_up=nullptr;
std::mutex g_setup_mutex;
std::uintptr_t g_exe_base=0;
std::uint32_t g_exe_size=0;
std::atomic<unsigned> g_draws{0},g_indexed{0},g_up{0},g_indexed_up{0};
// Independent draw-family sampling; rare UP paths are not hidden by
// thousands of indexed world draws. All counters reset each Present.
std::array<std::atomic<unsigned>,4> g_family_samples{};
// Bound original Clear + surface-transfer scene provenance to one source
// witness per operation and every eighth physical Present. A continuous
// 60fps game must not flood local private analysis logs with raw content.
std::array<std::atomic<unsigned>,7> g_original_mutation_samples{};
// Successful game-owned target transitions: bounded per-real-Present evidence.
std::array<std::atomic<unsigned>,2> g_original_target_bind_samples{};
// Five original transform categories per authentic Set/Multiply route.
std::array<std::atomic<unsigned>,10> g_original_transform_samples{};
std::atomic<unsigned> g_original_vs_constant_samples{0u};
std::array<std::atomic<unsigned>,2> g_original_region_samples{};
// Four authentic unmirrored COM families, max one private witness per family
// in each sampled original physical Present (not per game mesh/vertex).
std::array<std::atomic<unsigned>,4> g_original_unmirrored_samples{};
// Exact real-device COM call totals per original physical Present. No
// original game patch handles, VB pointers, vertex counts or game data.
std::array<std::atomic<std::uint32_t>,4> g_original_unmirrored_calls{};
// Monotonic state-write revisions per PHYSICAL original source Present.
// Secondary devices and non-EXE renderer/mirror callbacks never advance.
std::array<std::atomic<std::uint32_t>,m2vr::drawprobe::kOriginalLineageKinds>
    g_original_lineage_writes{};
m2vr::drawprobe::DrawOriginGate g_draw_origin_gate{};
std::atomic<unsigned> g_present_count{0};
// Startup-only, pointer-free original COM boundary evidence. At most three
// flushed records for the FIRST authentic primary Draw and Present each.
// This pinpoints shutdown in probe preparation vs original COM vs XR dispatch,
// without logging game assets, stack addresses, shaders, camera or pixels.
std::atomic<bool> g_startup_original_draw_seen{false};
bool TraceFirstOriginalDrawEntry(unsigned family,
                                 std::uint64_t physical_present) noexcept {
    if(g_startup_original_draw_seen.exchange(
           true,std::memory_order_acq_rel))return false;
    m2vr::LogEvent("model2_d3d9_startup_original_draw_boundary",
        1u,family,physical_present,0u);
    m2vr::FlushLog();
    return true;
}
void TraceFirstOriginalDrawStage(bool selected,unsigned stage,
                                 unsigned family,std::uint64_t physical_present,
                                 std::uint32_t original_hresult=0u) noexcept {
    if(!selected)return;
    m2vr::LogEvent("model2_d3d9_startup_original_draw_boundary",
        stage,family,physical_present,original_hresult);
    m2vr::FlushLog();
}
void TraceFirstOriginalPresentStage(bool selected,unsigned stage,
                                    std::uint64_t physical_present,
                                    std::uint32_t original_hresult=0u) noexcept {
    if(!selected)return;
    m2vr::LogEvent("model2_d3d9_startup_original_present_boundary",
        stage,0u,physical_present,original_hresult);
    m2vr::FlushLog();
}

// Physical original Draw order across all four API families, including
// unsampled commands. Never inferred from per-family sample cadence.
std::atomic<std::uint64_t> g_original_frame_draw_ordinal{0u};
// Monotonic original EXE caller signatures across ALL four real Draw families.
// Never reset at a Present: a 1-in-1024 source sample could otherwise miss
// an UP-only title/sky/HUD caller reused by a supposedly race-only adapter.
std::atomic<std::uint64_t> g_original_draw_caller_bloom_lo{0u};
std::atomic<std::uint64_t> g_original_draw_caller_bloom_hi{0u};
std::atomic<bool> g_original_draw_caller_unknown{false};
m2vr::drawprobe::OriginalDrawCallerInventory g_original_caller_inventory{};
// Per-process original emulator run witness, NOT game-world authentication.
// Windows CSPRNG supplies an unpredictable 64-bit identifier once per native
// source process; a failed RNG leaves zero and the PRIVATE auditor fails closed.
// No process ID, machine identity, memory address or original assets exported.
std::uint64_t OriginalPrivateCaptureRunNonce() noexcept {
    static const std::uint64_t nonce=[]() noexcept {
        std::uint64_t value=0u;
        if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),
            static_cast<ULONG>(sizeof(value)),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
            return std::uint64_t{0u};
        return value;
    }();
    return nonce;
}
// CallerRva is defined below, after the original-game module/image gate.
std::uint32_t CallerRva(void* ret);
void ObserveOriginalAllFamilyDrawCaller(void* origin,
    std::uint64_t physical_frame,std::uint64_t original_ordinal,
    std::uint32_t family) noexcept {
    const auto rva=CallerRva(origin);
    if(!rva || !physical_frame || physical_frame>0xffffffffu ||
       !original_ordinal || original_ordinal>10000000u ||
       family<1u || family>4u){
        g_original_draw_caller_unknown.store(true,std::memory_order_release);
        return;
    }
    const auto bits=m2vr::drawprobe::OriginalDrawCallerBits(rva);
    g_original_draw_caller_bloom_lo.fetch_or(
        bits.lane0,std::memory_order_relaxed);
    g_original_draw_caller_bloom_hi.fetch_or(
        bits.lane1,std::memory_order_relaxed);
    const auto state=g_original_caller_inventory.Observe(rva);
    if(state==m2vr::drawprobe::OriginalCallerInsert::NewCaller)
        m2vr::LogEvent("model2_d3d9_original_draw_caller_first",
            physical_frame,rva,
            (std::uint64_t{family}<<32u)|original_ordinal,0u);
    else if(state==m2vr::drawprobe::OriginalCallerInsert::Incomplete)
        g_original_draw_caller_unknown.store(true,std::memory_order_release);
}
// Optional successful original BeginScene/EndScene provenance, never a
// substitute for independently authenticated race-world/HUD ownership.
std::atomic<unsigned> g_original_scene_ordinal{0u};
std::atomic<bool> g_original_scene_open{false};
// A failed genuine original scene COM transition invalidates later scene
// provenance until physical Present / Reset; do not reopen in this epoch.
std::atomic<bool> g_original_scene_poisoned{false};
std::atomic<DWORD> g_original_scene_owner_thread{0u};
std::atomic<bool> g_original_scene_hooks_ready{false};
m2vr::drawprobe::OriginalPhysicalSceneDrawCoverage g_original_scene_draw_coverage{};
m2vr::drawprobe::OriginalPhysicalTargetDrawCoverage g_original_target_draw_coverage{true};
m2vr::drawprobe::OriginalCameraComLifetime g_original_camera_lifetime{};
// Read-only COM binding identities and descriptors, NEVER original texels.
// No source surface pointer is serialized, exported or retained after call.
m2vr::drawprobe::OriginalTargetSnapshot ReadPhysicalOriginalRtDepth(
    IDirect3DDevice9* d) noexcept {
    using namespace m2vr::drawprobe;
    OriginalTargetSnapshot result{};
    if(!d)return result;
    IDirect3DSurface9* rt=nullptr;
    IDirect3DSurface9* back=nullptr;
    IDirect3DSurface9* depth=nullptr;
    if(SUCCEEDED(d->GetRenderTarget(0u,&rt)) && rt){
        if(SUCCEEDED(d->GetBackBuffer(0u,0u,D3DBACKBUFFER_TYPE_MONO,
                                      &back)) && back)
            result.target=(rt==back)?OriginalPhysicalRtClass::Backbuffer:
                OriginalPhysicalRtClass::Auxiliary;
        if(SUCCEEDED(d->GetDepthStencilSurface(&depth)) && depth){
            D3DSURFACE_DESC rt_desc{},depth_desc{};
            const bool known=SUCCEEDED(rt->GetDesc(&rt_desc)) &&
                SUCCEEDED(depth->GetDesc(&depth_desc));
            if(known)result.depth=OriginalPhysicalDepthCompatible(
                rt_desc.Width,rt_desc.Height,
                static_cast<std::uint32_t>(rt_desc.MultiSampleType),
                rt_desc.MultiSampleQuality,depth_desc.Width,depth_desc.Height,
                static_cast<std::uint32_t>(depth_desc.MultiSampleType),
                depth_desc.MultiSampleQuality)?
                OriginalPhysicalDepthClass::Compatible:
                OriginalPhysicalDepthClass::Incompatible;
        }else result.depth=OriginalPhysicalDepthClass::Incompatible;
    }
    if(depth)depth->Release();
    if(back)back->Release();
    if(rt)rt->Release();
    return result;
}
// First unsampled original Draw after hooking, Reset or StateBlock Apply
// must check all actually supported MRT slots, NOT assume 1..3 are empty.
// COM interface refs from GetRenderTarget are released on every exit.
m2vr::drawprobe::OriginalSecondaryRtSnapshot ReadPhysicalOriginalSecondaryRt(
    IDirect3DDevice9* d) noexcept {
    m2vr::drawprobe::OriginalSecondaryRtSnapshot result{};
    if(!d)return result;
    D3DCAPS9 caps{};
    if(FAILED(d->GetDeviceCaps(&caps)) ||
       caps.NumSimultaneousRTs<1u || caps.NumSimultaneousRTs>4u)
        return result;
    result.known=true;
    for(DWORD slot=1u;slot<caps.NumSimultaneousRTs;++slot){
        IDirect3DSurface9* surface=nullptr;
        // Use the original function pointer if installed; an internal
        // read-only query must not impersonate an original EXE GetRT call.
        const HRESULT hr=g_get_render_target?
            g_get_render_target(d,slot,&surface):
            d->GetRenderTarget(slot,&surface);
        if(hr==D3DERR_NOTFOUND && !surface)
            continue; // genuine D3D9 unbound valid slot
        if(SUCCEEDED(hr) && surface)
            result.mask|=1u<<(slot-1u);
        else result.known=false; // INVALIDCALL/device lost/partial read
        if(surface)surface->Release();
    }
    return result;
}
// Read-only physical original D3D9 source region: a compatible RT0/depth
// alone does not prove that the actual Draw fills a canonical World3D eye.
// Cache only per physical RT/depth/view-state revision; no source pixels.
m2vr::drawprobe::OriginalRasterSnapshot ReadPhysicalOriginalRaster(
    IDirect3DDevice9* d) noexcept {
    m2vr::drawprobe::OriginalRasterSnapshot result{};
    if(!d)return result;
    IDirect3DSurface9* rt=nullptr;
    D3DSURFACE_DESC desc{};
    const bool target=SUCCEEDED(d->GetRenderTarget(0u,&rt)) && rt &&
        SUCCEEDED(rt->GetDesc(&desc));
    if(rt)rt->Release();
    D3DVIEWPORT9 vp{};
    DWORD zen=0u,zwrite=0u,scissor=0u;
    DWORD color_write=0u,stencil=0u,z_func=0u,clip_planes=0u;
    const bool known=target && desc.Width>0u && desc.Height>0u &&
        desc.Width<=16384u && desc.Height<=16384u &&
        SUCCEEDED(d->GetViewport(&vp)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_ZENABLE,&zen)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_COLORWRITEENABLE,&color_write)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_STENCILENABLE,&stencil)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_ZFUNC,&z_func)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_CLIPPLANEENABLE,&clip_planes));
    if(!known)return result;
    result.known=true;
    bool full_scissor=scissor==0u;
    if(scissor==1u){
        RECT rect{};
        if(SUCCEEDED(d->GetScissorRect(&rect)))
            full_scissor=rect.left==0 && rect.top==0 &&
                rect.right==static_cast<LONG>(desc.Width) &&
                rect.bottom==static_cast<LONG>(desc.Height);
        else result.known=false;
    }
    result.full_main_depth_view=vp.X==0u && vp.Y==0u &&
        vp.Width==desc.Width && vp.Height==desc.Height &&
        vp.MinZ==0.0f && vp.MaxZ==1.0f &&
        zen==D3DZB_TRUE && zwrite==TRUE && full_scissor &&
        m2vr::drawprobe::OriginalPhysicalDirectEyeWriteProfile(
            color_write,stencil,z_func,clip_planes);
    return result;
}
void EnsurePhysicalOriginalRtDepth(IDirect3DDevice9* d) noexcept {
    const bool primary=g_original_target_draw_coverage.NeedsResolution();
    const bool secondary=g_original_target_draw_coverage.NeedsSecondaryResolution();
    const bool raster=g_original_target_draw_coverage.NeedsRasterResolution();
    if(!primary && !secondary && !raster)return; // cache by source revision
    // Real original D3D9 COM getters, not just a revision-only post-read
    // snapshot: a concurrent Draw, Set, Reset or physical Present invalidates
    // the WHOLE group before it can certify any CPU/Win64 OpenXR eye source.
    const auto ticket=g_original_target_draw_coverage.EnterSourceRead();
    if(!ticket.valid)return;
    const auto main=primary?ReadPhysicalOriginalRtDepth(d):
        m2vr::drawprobe::OriginalTargetSnapshot{};
    const auto mrt=secondary?ReadPhysicalOriginalSecondaryRt(d):
        m2vr::drawprobe::OriginalSecondaryRtSnapshot{};
    const auto region=raster?ReadPhysicalOriginalRaster(d):
        m2vr::drawprobe::OriginalRasterSnapshot{};
    g_original_target_draw_coverage.CompleteSourceRead(
        ticket,primary,main,secondary,mrt,raster,region);
}
bool OriginalSceneStillOwnedAfterDraw(
    unsigned source_scene,bool source_scene_open) noexcept {
    return m2vr::drawprobe::OriginalDrawRetainedSourceScene(
        source_scene_open,
        g_original_scene_open.load(std::memory_order_acquire),
        !g_original_scene_poisoned.load(std::memory_order_acquire),
        source_scene,
        g_original_scene_ordinal.load(std::memory_order_acquire),
        g_original_scene_owner_thread.load(std::memory_order_acquire)==
            GetCurrentThreadId()) &&
        g_original_scene_hooks_ready.load(std::memory_order_acquire);
}
std::atomic<std::uint64_t> g_witness_serial{0};
// Only frames containing a real sampled original VB/IB Draw receive a
// matching physical original Present outcome. No per-Draw GPU readback.
std::atomic<unsigned> g_original_source_result_samples{0u};
std::uint32_t g_draw_sample_every=1024u;
std::atomic<bool> g_device_installed{false};
m2vr::drawprobe::OriginalD3D9DeviceOwner g_primary_device{};
std::atomic<unsigned> g_secondary_presents{0u};
bool g_native_stereo=false;
bool g_query_hooks_enabled=false;
// Internal callback into the independently proven CPU-world renderer.
// No verified-world frame is generated or self-authorized by this probe.
extern "C" BOOL __stdcall Model2VR_CompleteVerifiedWorldOriginalPresent(
    unsigned long long,unsigned int,unsigned int);
extern "C" BOOL __stdcall Model2VR_HasVerifiedWorldPendingForOriginalPresent(
    unsigned long long);
extern "C" void __stdcall Model2VR_DiscardPendingWorldOnReset();
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Draw(
    std::uint32_t family,std::uint32_t result_bits,std::uint32_t caller_rva);
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Scene(
    std::uint32_t transition,std::uint32_t result_bits,
    std::uint32_t caller_rva);
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Clear(
    std::uint32_t flags,std::uint32_t result_bits,
    std::uint32_t caller_rva);
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Surface(
    std::uint32_t method,std::uint32_t result_bits,
    std::uint32_t caller_rva);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9TargetDrawCoverage(
    unsigned long long,unsigned int,unsigned int,unsigned int);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9CameraComLifetime(
    unsigned long long,unsigned int,unsigned int,unsigned int);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9StateBlockMutation(
    unsigned long long physical_present,unsigned int stage,
    unsigned int original_caller_rva,unsigned int original_hresult);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9InstancedStream(
    unsigned long long physical_present,unsigned int stream,
    unsigned int original_caller_rva,unsigned int frequency);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9ActiveInstancing(
    unsigned long long physical_present,unsigned int active_stream_mask);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9SwapChainPresent(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned int reason);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9QueryUse(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int reason);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9ValidationUse(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int reason);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9ManagedResidency(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int reason);
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9BufferMutation(
    unsigned long long physical_present,unsigned int caller_rva,
    unsigned long long ordinal,unsigned int family);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9BufferHookCoverage(
    unsigned long long physical_present,unsigned int caller_rva,
    unsigned int family);
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9TextureUpload(
    unsigned long long physical_present,unsigned int caller_rva,
    unsigned long long preceding_draw_ordinal);
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9GpuSurfaceMutation(
    unsigned long long physical_present,unsigned int caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int method);
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9PresentationMutation(
    unsigned long long physical_present,unsigned int caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int method);
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9PaletteMutation(
    unsigned long long physical_present,unsigned int caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int method);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9RasterMode(
    unsigned long long physical_present,unsigned int unsafe_mask,
    unsigned int caller_rva);
extern "C" void __stdcall Model2VR_RejectOriginalD3D9UnmirroredGeometry(
    unsigned long long physical_present,unsigned int method,
    unsigned int caller_rva,unsigned long long ordinal,
    unsigned int original_hresult);

template<class T>
bool Hook(void* target,void* detour,T& original){
    if(!target)return false;
    const MH_STATUS status=MH_CreateHook(target,detour,
        reinterpret_cast<LPVOID*>(&original));
    if(status!=MH_OK)return false;
    if(MH_EnableHook(target)!=MH_OK){
        MH_RemoveHook(target);
        original=nullptr;
        return false;
    }
    return true;
}
std::uint32_t CallerRva(void* ret) {
    const auto raw=reinterpret_cast<std::uintptr_t>(ret);
    if(raw<g_exe_base || raw-g_exe_base>=g_exe_size)return 0;
    return static_cast<std::uint32_t>(raw-g_exe_base);
}
// Forward the REAL original COM return before any GPU-eye replay. Only
// failed original Draws notify an independently authenticated CPU-world
// transaction, so normal successful Draws incur no extra bridge lock.
void TrackFailedOriginalD3D9Scene(HRESULT hr,std::uint32_t transition,
                                  std::uint32_t original_caller_rva) noexcept {
    if(SUCCEEDED(hr) || !original_caller_rva)return;
    g_original_scene_poisoned.store(true,std::memory_order_release);
    g_original_scene_open.store(false,std::memory_order_release);
    g_original_scene_owner_thread.store(0u,std::memory_order_release);
    m2vr::LogEvent("model2_d3d9_original_scene_failed",
        g_present_count.load(std::memory_order_relaxed)+1u,
        transition,static_cast<std::uint32_t>(hr),original_caller_rva);
    Model2VR_RejectFailedOriginalD3D9Scene(
        transition,static_cast<std::uint32_t>(hr),original_caller_rva);
    // Drop the optional speculative native GPU stereo pair too. Keep the
    // actual original game mono backbuffer for the next physical Present.
    if(g_native_stereo)
        m2vr::daytona::MarkFailedOriginalScene(
            transition,static_cast<std::uint32_t>(hr));
}
void LogOriginalD3D9MutationPhase(std::uint32_t method,
                                  std::uint32_t caller_rva,
                                  HRESULT hr)noexcept{
    if(!caller_rva || method<1u || method>7u || FAILED(hr))return;
    const auto frame=g_present_count.load(std::memory_order_relaxed)+1u;
    if(frame!=1u && frame%8u!=0u)return;
    const auto prior=g_original_mutation_samples[method-1u].fetch_add(
        1u,std::memory_order_relaxed);
    if(prior!=0u)return;
    // Kind 1=Clear, 2=UpdateSurface, 3=StretchRect, 4=ColorFill,
    // 5=UpdateTexture (31), 6=GetRenderTargetData (32),
    // 7=GetFrontBufferData (33). Source schedule only, no texels.
    // a0=original Present, a1=EXE-relative return caller,
    // a2=(kind << 32)|all-family original Draw ordinal,
    // a3=(original scene ordinal << 32)|genuine scene-open flag.
    // Only source scheduling metadata, never game matrices/pixels/ROM.
    const auto draw=g_original_frame_draw_ordinal.load(std::memory_order_relaxed);
    const auto scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    const bool open=g_original_scene_hooks_ready.load(std::memory_order_acquire) &&
       !g_original_scene_poisoned.load(std::memory_order_acquire) &&
       g_original_scene_open.load(std::memory_order_acquire) &&
       g_original_scene_owner_thread.load(std::memory_order_acquire)==GetCurrentThreadId();
    if(draw>10000000ull || scene>100000u)return;
    m2vr::LogEvent("model2_d3d9_original_mutation_phase",
        frame,caller_rva,(std::uint64_t(method)<<32)|draw,
        (std::uint64_t(scene)<<32)|(open?1ull:0ull));
}
// Original successful primary-device target/depth binding evidence. The
// source's target identity/format is never disclosed; only RT0 backbuffer
// vs auxiliary and depth/target dimension match metadata are sampled.
void LogOriginalD3D9TargetBindPhase(IDirect3DDevice9* d,
    std::uint32_t method,std::uint32_t caller_rva,DWORD index,
    IDirect3DSurface9* surface,HRESULT hr)noexcept{
    if(!d || !caller_rva || method<1u || method>2u || FAILED(hr))return;
    const auto frame=g_present_count.load(std::memory_order_relaxed)+1u;
    if(frame!=1u && frame%8u!=0u)return;
    if(g_original_target_bind_samples[method-1u].fetch_add(
        1u,std::memory_order_relaxed)!=0u)return;
    unsigned target_class=0u;
    bool desc_known=false,matching_size=false;
    if(method==1u && index==0u && surface){
        IDirect3DSurface9* backbuffer=nullptr;
        if(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,
                                     &backbuffer)) && backbuffer){
            target_class=surface==backbuffer?1u:2u;
            backbuffer->Release();
        }
    }else if(method==2u && surface){
        IDirect3DSurface9* target=nullptr;
        D3DSURFACE_DESC depth_desc{},target_desc{};
        const bool d_ok=SUCCEEDED(surface->GetDesc(&depth_desc));
        const bool t_ok=SUCCEEDED(d->GetRenderTarget(0,&target))&&target &&
            SUCCEEDED(target->GetDesc(&target_desc));
        desc_known=d_ok&&t_ok;
        matching_size=desc_known && depth_desc.Width==target_desc.Width &&
            depth_desc.Height==target_desc.Height;
        if(target)target->Release();
    }
    const auto ordinal=g_original_frame_draw_ordinal.load(
        std::memory_order_relaxed);
    const auto scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    if(ordinal>10000000u || scene>100000u)return;
    const bool scene_open=g_original_scene_hooks_ready.load(
        std::memory_order_acquire) &&
        !g_original_scene_poisoned.load(std::memory_order_acquire) &&
        g_original_scene_open.load(std::memory_order_acquire) &&
        g_original_scene_owner_thread.load(std::memory_order_acquire)==
            GetCurrentThreadId();
    // a0=physical Present, a1=EXE caller RVA, a2=(bind method<<32)|Draw
    // ordinal, a3=(scene ordinal<<32)|flags: bit0 scene open,
    // bits8..9 RT0 class, bit16 descriptors known, bit17 size match.
    const std::uint32_t flags=(scene_open?1u:0u)|
        (target_class<<8u)|(desc_known?1u<<16u:0u)|
        (matching_size?1u<<17u:0u);
    m2vr::LogEvent("model2_d3d9_original_target_bind_phase",
        frame,caller_rva,(std::uint64_t{method}<<32u)|ordinal,
        (std::uint64_t{scene}<<32u)|flags);
}
// Genuine original EXE-owned viewport/scissor source, not World3D proof.
// Only relative, quantized RT0 geometry is emitted: no raw pixels/coordinates.
void LogOriginalRegionPhase(IDirect3DDevice9* d,unsigned method,
    std::uint32_t rva,std::int64_t x,std::int64_t y,
    std::int64_t w,std::int64_t h)noexcept {
    if(!d || !rva || method<1u || method>2u)return;
    const auto frame=g_present_count.load(std::memory_order_relaxed)+1u;
    if(frame!=1u && frame%8u!=0u)return;
    // Four first source region writes per family expose mid-frame HUD/sky
    // transitions without unbounded original-game logging.
    const auto source_write=g_original_region_samples[method-1u].fetch_add(
        1u,std::memory_order_relaxed);
    if(source_write>=4u)return;
    const auto draw=g_original_frame_draw_ordinal.load(std::memory_order_relaxed);
    const auto scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    if(draw>10000000u || scene>100000u)return;
    const bool open=g_original_scene_hooks_ready.load(std::memory_order_acquire) &&
        !g_original_scene_poisoned.load(std::memory_order_acquire) &&
        g_original_scene_open.load(std::memory_order_acquire) &&
        g_original_scene_owner_thread.load(std::memory_order_acquire)==GetCurrentThreadId();
    IDirect3DSurface9* target=nullptr;
    IDirect3DSurface9* backbuffer=nullptr;
    D3DSURFACE_DESC desc{};
    unsigned tw=0u,th=0u,kind=0u;
    if(SUCCEEDED(d->GetRenderTarget(0,&target)) && target){
        if(SUCCEEDED(target->GetDesc(&desc))){tw=desc.Width;th=desc.Height;}
        if(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,
                                      &backbuffer)) && backbuffer)
            kind=target==backbuffer?1u:2u;
    }
    if(backbuffer)backbuffer->Release();
    if(target)target->Release();
    DWORD scissor=0;
    const bool enabled=SUCCEEDED(d->GetRenderState(
        D3DRS_SCISSORTESTENABLE,&scissor)) && scissor!=0u;
    const bool known=tw!=0u && th!=0u;
    const bool valid=known && w>0 && h>0;
    const bool inside=valid && x>=0 && y>=0 &&
        x+w<=static_cast<std::int64_t>(tw) &&
        y+h<=static_cast<std::int64_t>(th);
    const bool full=inside && x==0 && y==0 && w==tw && h==th;
    auto bin=[](std::int64_t n,unsigned den)->std::uint32_t {
        if(n<=0 || !den)return 0u;
        if(static_cast<std::uint64_t>(n)>=den)return 31u;
        return static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(n)*31u+den/2u)/den);
    };
    std::uint32_t flags=(open?1u:0u)|(known?2u:0u)|
        (valid?4u:0u)|(inside?8u:0u)|(full?16u:0u)|
        (enabled?32u:0u)|((known && kind==1u)?64u:0u)|
        ((known && kind==2u)?128u:0u);
    if(valid)flags|=(bin(x,tw)<<8u)|(bin(y,th)<<13u)|
                    (bin(w,tw)<<18u)|(bin(h,th)<<23u);
    // Upper nibble = original write ordinal 0..3 in this sampled frame.
    flags|=source_write<<28u;
    // a0=original Present; a1=original caller RVA;
    // a2=(method<<32)|all-family Draw ordinal;
    // a3=(scene ordinal<<32)|relative region flags and 0..3 write ordinal.
    m2vr::LogEvent("model2_d3d9_original_region_phase",
        frame,rva,(std::uint64_t{method}<<32u)|draw,
        (std::uint64_t{scene}<<32u)|flags);
}
m2vr::drawprobe::OriginalLineageStamp ReadOriginalLineage(
    std::uint64_t physical_frame)noexcept{
    m2vr::drawprobe::OriginalLineageStamp stamp{};
    stamp.physical_present=physical_frame;
    for(std::size_t i=0;i<stamp.successful_writes.size();++i)
        stamp.successful_writes[i]=g_original_lineage_writes[i].load(
            std::memory_order_acquire);
    if(g_present_count.load(std::memory_order_acquire)+1u!=physical_frame)
        return {}; // concurrent physical original Present, unknown.
    return stamp;
}
void RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind kind,
                                std::uint32_t rva,HRESULT hr)noexcept {
    if(!rva || FAILED(hr))return;
    auto& counter=g_original_lineage_writes[static_cast<std::size_t>(kind)];
    auto old=counter.load(std::memory_order_relaxed);
    while(old<=m2vr::drawprobe::kMaxLineageWritesPerPresent &&
          !counter.compare_exchange_weak(old,old+1u,
              std::memory_order_acq_rel,std::memory_order_relaxed)){}
}
// The actual original destination may be passed directly to a GPU
// transfer before Create*/Get*/SetRenderTarget ever reveals its Surface.
// Install existing CPU/GDI hooks before the next map (no pixel inspection).
bool ObserveOriginalTextureSurface(IDirect3DSurface9* surf,
                                   std::uint32_t caller_rva);
void OriginalTextureCoverageUnknown(std::uint32_t caller_rva) noexcept;
void ObserveOriginalGpuSurfaceDestination(IDirect3DSurface9* destination,
                                          std::uint32_t caller_rva) noexcept {
    if(!caller_rva)return;
    if(!destination || !ObserveOriginalTextureSurface(destination,caller_rva))
        OriginalTextureCoverageUnknown(caller_rva);
}
// A GPU write may race an already staged speculative CPU or native GPU
// OpenXR eye pair, even when no CPU LockRect/texture binding occurs. Revoke
// before executing the real original API, not only after observing success.
// Original Draw, stereo authorization and mono gameplay remain unchanged.
void BeforeOriginalGpuSurfaceWrite(std::uint32_t caller_rva,
                                   std::uint32_t method) noexcept {
    if(!caller_rva || method<1u || method>4u)return;
    Model2VR_ObserveOriginalD3D9GpuSurfaceMutation(
        g_present_count.load(std::memory_order_acquire)+1u,caller_rva,
        g_original_frame_draw_ordinal.load(std::memory_order_acquire),method);
}
void RecordOriginalGpuSurfaceWrite(std::uint32_t caller_rva,HRESULT hr,
                                   std::uint32_t family) noexcept {
    if(!caller_rva || FAILED(hr) || family<1u || family>4u)return;
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalLineageKind::GpuSurfaceContents,caller_rva,hr);
    g_original_gpu_surface_writes[family-1u].fetch_add(
        1u,std::memory_order_relaxed);
}
void TrackFailedOriginalD3D9Clear(HRESULT hr,std::uint32_t flags,
                                  std::uint32_t caller_rva) noexcept {
    if(!m2vr::vrbridge::IsFailedOriginalD3D9Clear(
           static_cast<std::uint32_t>(hr)) || !caller_rva)return;
    g_original_scene_poisoned.store(true,std::memory_order_release);
    g_original_scene_open.store(false,std::memory_order_release);
    g_original_scene_owner_thread.store(0u,std::memory_order_release);
    m2vr::LogEvent("model2_d3d9_original_clear_failed",
        g_present_count.load(std::memory_order_relaxed)+1u,
        flags,static_cast<std::uint32_t>(hr),caller_rva);
    Model2VR_RejectFailedOriginalD3D9Clear(
        flags,static_cast<std::uint32_t>(hr),caller_rva);
    if(g_native_stereo)
        m2vr::daytona::MarkFailedOriginalClear(
            flags,static_cast<std::uint32_t>(hr));
}
void TrackFailedOriginalD3D9Surface(HRESULT hr,std::uint32_t method,
                                    std::uint32_t caller_rva)noexcept {
    if(!m2vr::vrbridge::IsFailedOriginalD3D9Surface(
           method,static_cast<std::uint32_t>(hr)) || !caller_rva)return;
    g_original_scene_poisoned.store(true,std::memory_order_release);
    g_original_scene_open.store(false,std::memory_order_release);
    g_original_scene_owner_thread.store(0u,std::memory_order_release);
    m2vr::LogEvent("model2_d3d9_original_surface_failed",
        g_present_count.load(std::memory_order_relaxed)+1u,
        method,static_cast<std::uint32_t>(hr),caller_rva);
    Model2VR_RejectFailedOriginalD3D9Surface(
        method,static_cast<std::uint32_t>(hr),caller_rva);
    if(g_native_stereo)
        m2vr::daytona::MarkFailedOriginalSurface(
            method,static_cast<std::uint32_t>(hr));
}
HRESULT TrackOriginalD3D9DrawResult(HRESULT hr,std::uint32_t family,
                                    void* caller,IDirect3DDevice9* device) noexcept {
    // Only authentic game EXE calls on the primary device can poison
    // this physical Present. Ignore secondary, overlay and mirror calls.
    if(!m2vr::vrbridge::IsFailedOriginalD3D9Draw(
           family,static_cast<std::uint32_t>(hr)) ||
       !g_primary_device.Owns(device))return hr;
    const auto original_caller_rva=CallerRva(caller);
    if(!original_caller_rva)return hr;
    const auto frame=g_present_count.load(std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_d3d9_original_draw_failed",frame,
        family,static_cast<std::uint32_t>(hr),original_caller_rva);
    Model2VR_RejectFailedOriginalD3D9Draw(
        family,static_cast<std::uint32_t>(hr),original_caller_rva);
    // Even an unsampled failed original Draw discards speculative eyes.
    // Original game Draw/Present/FFB execute only once; retain mono backup.
    if(g_native_stereo)
        m2vr::daytona::MarkFailedOriginalDraw(
            family,static_cast<std::uint32_t>(hr));
    return hr;
}
bool Sample(IDirect3DDevice9* dev,unsigned family,
            D3DPRIMITIVETYPE topology,UINT primitive_count,void* caller){
    if(family<1u || family>4u)return false;
    // Preserve the first/1024th draw and up to 64 novel in-EXE sources,
    // while explicitly sampling mid-frame Draws with the bounded opt-in
    // cadence. The first-only Daytona log exposed a 2D blit but missed
    // up to hundreds of later DrawPrimitiveUP calls within each frame.
    // This exposes rare actual world draw callsites without GPU readbacks.
    const unsigned ordinal=g_family_samples[family-1].fetch_add(
        1,std::memory_order_relaxed);
    const auto caller_rva=CallerRva(caller);
    if(!g_draw_origin_gate.ShouldObserve(family,caller_rva,ordinal) &&
       (ordinal%g_draw_sample_every)!=0u)return false;
    const std::uint64_t topology_and_count=
        (std::uint64_t{primitive_count}<<32)|
        static_cast<std::uint32_t>(topology);
    // a0=frame about to Present, a1=draw API family (1..4),
    // a2=caller RVA (0 means unverified/outside EXE), a3=(count<<32)|type.
    // It is a caller observation, not world/HUD/mirror classification.
    const auto upcoming_frame=g_present_count.load(
        std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_d3d9_draw_origin",
        upcoming_frame,family,caller_rva,topology_and_count);
    // Relative Draw position lets offline analysis distinguish early
    // full-screen blits from late UI/HUD overlays at the same caller RVA.
    m2vr::LogEvent("model2_d3d9_draw_ordinal",
        upcoming_frame,family,ordinal,caller_rva);
    DWORD fvf=0,z=0,alpha=0;
    const bool ok=SUCCEEDED(dev->GetFVF(&fvf)) &&
                  SUCCEEDED(dev->GetRenderState(D3DRS_ZENABLE,&z)) &&
                  SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha));
    if(!ok){m2vr::LogEvent("model2_d3d9_sample_failed",family);return true;}
    // A sample shows whether pretransformed (XYZRHW) data dominates.
    // It is NEVER sufficient by itself to classify world geometry.
    // Port0r's separate secondary projection regions motivate recording
    // viewport, scissor and target provenance; none alone proves a world pass.
    DWORD scissor_enabled=0;
    const bool scissor_state_ok=SUCCEEDED(
        dev->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor_enabled));
    D3DVIEWPORT9 viewport{};
    const bool viewport_ok=SUCCEEDED(dev->GetViewport(&viewport));
    RECT scissor{};
    const bool scissor_ok=scissor_state_ok && scissor_enabled &&
        SUCCEEDED(dev->GetScissorRect(&scissor));

    IDirect3DSurface9* target=nullptr;
    IDirect3DSurface9* backbuffer=nullptr;
    const bool target_ok=SUCCEEDED(dev->GetRenderTarget(0,&target)) && target;
    const bool backbuffer_ok=SUCCEEDED(dev->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer)) && backbuffer;
    D3DSURFACE_DESC desc{};
    const bool desc_ok=target_ok && SUCCEEDED(target->GetDesc(&desc));
    // Unknown if either COM query fails; never claim auxiliary without a
    // known main backbuffer comparison. Release all acquired COM references.
    const std::uint32_t target_class=target_ok && backbuffer_ok ?
        (target==backbuffer ? 1u : 2u) : 0u; // unknown/main/aux
    if(target)target->Release();
    if(backbuffer)backbuffer->Release();

    const std::uint32_t packed=(z?1u:0u)|(alpha?2u:0u)|
        ((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW?4u:0u)|
        (scissor_state_ok && scissor_enabled?8u:0u)|
        (viewport_ok?16u:0u)|(scissor_ok?32u:0u)|
        (target_class!=0u?64u:0u)|(target_class==1u?128u:0u);
    m2vr::LogEvent("model2_d3d9_draw_sample",family,fvf,packed,caller_rva);
    if(viewport_ok){
        const std::uint64_t size=(std::uint64_t{viewport.Width}<<32)|
            std::uint64_t{viewport.Height};
        m2vr::LogEvent("model2_d3d9_viewport_sample",caller_rva,
            viewport.X,viewport.Y,size);
    }
    if(scissor_ok){
        const std::uint64_t edges=
            (std::uint64_t{static_cast<std::uint32_t>(scissor.right)}<<32)|
            static_cast<std::uint32_t>(scissor.bottom);
        m2vr::LogEvent("model2_d3d9_scissor_sample",caller_rva,
            static_cast<std::uint32_t>(scissor.left),
            static_cast<std::uint32_t>(scissor.top),edges);
    }
    m2vr::LogEvent("model2_d3d9_target_sample",caller_rva,target_class,
        desc_ok?desc.Width:0u,desc_ok?desc.Height:0u);
    return true;
}

// Each independently sampled original Draw witness retains the original
// source RT/viewport/depth context and the *upcoming* original Present id.
// This records evidence only: unknown/aux/HUD never becomes world proof.
void LogOriginalSourcePassContext(IDirect3DDevice9* dev,
                                  std::uint64_t witness) {
    if(!dev || !witness)return;
    using namespace m2vr::drawprobe;
    OriginalDrawSourceContext c{};
    IDirect3DSurface9* source=nullptr;
    IDirect3DSurface9* backbuffer=nullptr;
    const bool have_source=SUCCEEDED(dev->GetRenderTarget(0,&source))&&source;
    const bool have_backbuffer=SUCCEEDED(dev->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer))&&backbuffer;
    D3DSURFACE_DESC source_desc{};
    D3DSURFACE_DESC backbuffer_desc{};
    c.target_desc_known=have_source &&
        SUCCEEDED(source->GetDesc(&source_desc));
    const bool backbuffer_desc_known=have_backbuffer &&
        SUCCEEDED(backbuffer->GetDesc(&backbuffer_desc));
    if(c.target_desc_known){
        c.source_width=source_desc.Width;
        c.source_height=source_desc.Height;
    }
    if(have_source && have_backbuffer)
        c.target=source==backbuffer?
            OriginalTargetClass::MainBackbuffer:
            OriginalTargetClass::Auxiliary;
    if(source)source->Release();
    if(backbuffer)backbuffer->Release();
    D3DVIEWPORT9 vp{};
    c.viewport_known=SUCCEEDED(dev->GetViewport(&vp));
    c.viewport_full=c.viewport_known && backbuffer_desc_known &&
        vp.X==0u && vp.Y==0u &&
        vp.Width==backbuffer_desc.Width &&
        vp.Height==backbuffer_desc.Height &&
        vp.MinZ==0.f && vp.MaxZ==1.f;
    DWORD z=0u,write=0u,scissor=0u;
    c.depth_known=SUCCEEDED(dev->GetRenderState(D3DRS_ZENABLE,&z)) &&
        SUCCEEDED(dev->GetRenderState(D3DRS_ZWRITEENABLE,&write));
    c.depth_enabled=c.depth_known && z==D3DZB_TRUE;
    c.depth_write=c.depth_known && write!=0u;
    c.scissor_known=SUCCEEDED(
        dev->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor));
    // Treat any nonzero original value as enabled for provenance.
    // Unknown values must fail closed, never masquerade as disabled.
    c.scissor_enabled=c.scissor_known && scissor!=0u;
    if(c.scissor_known && scissor==1u){
        RECT source_scissor{};
        c.scissor_rect_known=SUCCEEDED(dev->GetScissorRect(&source_scissor));
        c.scissor_full=c.scissor_rect_known && c.target_desc_known &&
            c.target==OriginalTargetClass::MainBackbuffer &&
            FullBackbufferScissor(scissor,&source_scissor,source_desc);
    }
    IDirect3DSurface9* depth=nullptr;
    if(SUCCEEDED(dev->GetDepthStencilSurface(&depth)) && depth){
        D3DSURFACE_DESC depth_desc{};
        c.depth_surface_known=SUCCEEDED(depth->GetDesc(&depth_desc));
        c.depth_surface_compatible=c.depth_surface_known &&
            c.target_desc_known &&
            CompatibleOriginalDepthSurface(source_desc,depth_desc);
    }
    if(depth)depth->Release();
    const auto original_frame=g_present_count.load(
        std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_d3d9_source_pass_context",witness,
        original_frame,EncodeOriginalDrawSourceContext(c),
        (std::uint64_t{c.source_width}<<32)|c.source_height);
    // Same original Draw/Present ZFUNC and CULLMODE provenance, never
    // automatic world-pass authorization or a duplicate game Draw.
    DWORD zfunc=0u,cull=0u;
    if(c.depth_known &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ZFUNC,&zfunc)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE,&cull)))
        m2vr::LogEvent("model2_d3d9_source_depth_cull",witness,
            (std::uint64_t{z}<<32)|zfunc,
            (std::uint64_t{write}<<32)|cull,original_frame);
}

// Read-only fixed-function material state belongs to its exact original
// Draw witness, not a later Present or an inferred gameplay-world pass.
// No original D3D9 state or game/FFB tick is modified.
void LogOriginalFixedStageEvidence(IDirect3DDevice9* dev,
                                   std::uint64_t witness,DWORD fvf,UINT stride) {
    if(!dev || !witness)return;
    DWORD co=0,c1=0,c2=0,ao=0,a1=0,a2=0;
    DWORD next_color=0,next_alpha=0,specular=0,fog=0;
    DWORD current_result=0,next_result=0;
    if(FAILED(dev->GetTextureStageState(0,D3DTSS_COLOROP,&co)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_COLORARG1,&c1)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_COLORARG2,&c2)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_ALPHAOP,&ao)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_ALPHAARG1,&a1)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_ALPHAARG2,&a2)) ||
       FAILED(dev->GetTextureStageState(1,D3DTSS_COLOROP,&next_color)) ||
       FAILED(dev->GetTextureStageState(1,D3DTSS_ALPHAOP,&next_alpha)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_RESULTARG,&current_result)) ||
       FAILED(dev->GetTextureStageState(1,D3DTSS_RESULTARG,&next_result)) ||
       FAILED(dev->GetRenderState(D3DRS_SPECULARENABLE,&specular)) ||
       FAILED(dev->GetRenderState(D3DRS_FOGENABLE,&fog)))
        return; // Unavailable states are unknown, never guessed.
    m2vr::LogEvent("model2_d3d9_source_stage0_color",witness,
        (std::uint64_t{co}<<32)|c1,c2,
        (std::uint64_t{fvf}<<32)|stride);
    m2vr::LogEvent("model2_d3d9_source_stage0_alpha",witness,
        (std::uint64_t{ao}<<32)|a1,a2,
        (std::uint64_t{fvf}<<32)|stride);
    m2vr::LogEvent("model2_d3d9_source_stage1_specular",witness,
        next_color,next_alpha,(std::uint64_t{specular}<<32)|fog);
    // D3D9 stage0 RESULTARG=TEMP can make plausible diffuse/texel
    // operations non-equivalent to the final pixel. Preserve actual
    // source output register and declaration without enabling world proof.
    m2vr::LogEvent("model2_d3d9_source_stage_result",witness,
        current_result,next_result,(std::uint64_t{fvf}<<32)|stride);
    // Source lighting has no effect on original pretransformed XYZRHW,
    // but may change XYZ color; record it under the exact same Draw witness
    // without treating either original state as gameplay-world proof.
    DWORD lighting=0,color_vertex=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_LIGHTING,&lighting)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_COLORVERTEX,&color_vertex)))
        m2vr::LogEvent("model2_d3d9_source_lighting",witness,
            lighting,color_vertex,(std::uint64_t{fvf}<<32)|stride);
    // Capture original alpha compare on the SAME genuine Draw witness.
    // Preserve exact FVF/stride so a later or unrelated material sample
    // cannot be joined to this source or accidentally authorize a world.
    DWORD alpha_enabled=0u,alpha_function=0u,alpha_reference=0u;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha_enabled)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAFUNC,&alpha_function)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAREF,&alpha_reference)))
        m2vr::LogEvent("model2_d3d9_source_alpha_compare",witness,
            (std::uint64_t{alpha_enabled}<<32)|alpha_function,
            alpha_reference,(std::uint64_t{fvf}<<32)|stride);
    // Read original D3D9 blend factors under the SAME Draw witness.
    // Passive capture only; no input, FFB or original render modification.
    DWORD blend_enable=0,blend_src=0,blend_dst=0,blend_op=0;
    DWORD separate_alpha=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend_enable)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_SRCBLEND,&blend_src)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_DESTBLEND,&blend_dst)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_BLENDOP,&blend_op)) &&
       SUCCEEDED(dev->GetRenderState(
           D3DRS_SEPARATEALPHABLENDENABLE,&separate_alpha))) {
        m2vr::LogEvent("model2_d3d9_source_blend",witness,
            (std::uint64_t{blend_enable}<<32)|blend_src,
            (std::uint64_t{blend_dst}<<32)|blend_op,
            separate_alpha);
        // Independent ADD alpha factors MUST belong to this SAME original
        // Draw witness and FVF/stride, not a later Present or another pass.
        if(blend_enable==1u && separate_alpha==1u) {
            DWORD source_write_mask=0u;
            // The original alpha blend registers are semantically inert
            // if the genuine source COLORWRITEENABLE masks ALPHA. Avoid
            // unnecessary COM reads; the SAME-witness original color-mask
            // event still certifies this exact RGB-only path separately.
            if(SUCCEEDED(dev->GetRenderState(
                    D3DRS_COLORWRITEENABLE,&source_write_mask)) &&
               (source_write_mask&D3DCOLORWRITEENABLE_ALPHA)!=0u) {
                DWORD asrc=0u,adst=0u,aop=0u;
                if(SUCCEEDED(dev->GetRenderState(
                               D3DRS_SRCBLENDALPHA,&asrc)) &&
                   SUCCEEDED(dev->GetRenderState(
                               D3DRS_DESTBLENDALPHA,&adst)) &&
                   SUCCEEDED(dev->GetRenderState(
                               D3DRS_BLENDOPALPHA,&aop)))
                    m2vr::LogEvent("model2_d3d9_source_separate_alpha_blend",
                        witness,(std::uint64_t{asrc}<<32)|adst,aop,
                        (std::uint64_t{fvf}<<32)|stride);
            }
        }
    }
    // Original fixed-function vertex stage for the SAME sampled Draw.
    // Neither source FVF nor a CPU projector match establishes the
    // unobserved shade/clip/vertex-blend state or race-world ownership.
    DWORD shade=0,clipping=0,clip_planes=0,vertex_blend=0,indexed_blend=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_SHADEMODE,&shade)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_CLIPPING,&clipping)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_CLIPPLANEENABLE,&clip_planes)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_VERTEXBLEND,&vertex_blend)) &&
       SUCCEEDED(dev->GetRenderState(
           D3DRS_INDEXEDVERTEXBLENDENABLE,&indexed_blend)))
        m2vr::LogEvent("model2_d3d9_source_vertex_pipeline",witness,
            (std::uint64_t{shade}<<32)|clipping,
            (std::uint64_t{clip_planes}<<32)|vertex_blend,
            (std::uint64_t{indexed_blend}<<32)|
             (g_present_count.load(std::memory_order_relaxed)+1u));
    // D3D9 TRIANGLELIST wireframe/point rendering changes pixel coverage.
    // The binocular world raster is filled-only: preserve the genuine
    // FILLMODE together with this very Draw's FVF and Present provenance.
    DWORD fill_mode=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_FILLMODE,&fill_mode)))
        m2vr::LogEvent("model2_d3d9_source_fill",witness,fill_mode,
            g_present_count.load(std::memory_order_relaxed)+1u,
            (std::uint64_t{fvf}<<32)|stride);
    // Source mip capability is read from the ACTUAL bound 2D texture at
    // this Draw witness. It cannot authorize an otherwise unverified Draw.
    DWORD source_mip_filter=0u;
    if(SUCCEEDED(dev->GetSamplerState(
           0,D3DSAMP_MIPFILTER,&source_mip_filter))){
        IDirect3DBaseTexture9* bound=nullptr;
        unsigned source_levels=0u;
        if(SUCCEEDED(dev->GetTexture(0,&bound)) && bound){
            if(bound->GetType()==D3DRTYPE_TEXTURE)
                source_levels=bound->GetLevelCount();
        }
        if(bound)bound->Release();
        m2vr::LogEvent("model2_d3d9_source_sampler_mip",witness,
            source_mip_filter,source_levels,
            g_present_count.load(std::memory_order_relaxed)+1u);
    }
    DWORD color_mask=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_COLORWRITEENABLE,&color_mask)))
        m2vr::LogEvent("model2_d3d9_source_color_mask",witness,
            color_mask,(std::uint64_t{fvf}<<32)|stride,
            g_present_count.load(std::memory_order_relaxed)+1u);
}

// Read at most 16 guarded bytes at a time from game-owned UP pointers.
// The small SEH helper contains no C++ objects requiring unwinding.
bool SafeReadBytes(const void* source,std::size_t offset,
                   void* destination,std::size_t size) noexcept {
    if(!source||!destination||!size||size>16u||
       offset>65536u-size)return false;
    const auto* p=static_cast<const volatile std::uint8_t*>(source);
    auto* out=static_cast<std::uint8_t*>(destination);
    __try {
        for(std::size_t j=0;j<size;++j)out[j]=p[offset+j];
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

// Candidate only: the actual fixed-function preprojection UP vertex,
 // stage-zero UV and original World/View matrices. Even D3DFVF_XYZ can be
 // HUD/UI or non-gameplay; this function NEVER asserts world proof or
 // publishes a GameStereo frame. Logs three bounded vertices at most.
std::uint64_t ObserveWorldUP(IDirect3DDevice9* dev,unsigned family,
                    D3DPRIMITIVETYPE type,UINT primitive_count,void* caller,
                    const void* vertices,UINT stride,const void* indices,
                    D3DFORMAT index_format,UINT first,UINT vertex_count) {
    using namespace m2vr::drawprobe;
    if(!dev || !vertices ||
       (type!=D3DPT_TRIANGLELIST &&
        type!=D3DPT_TRIANGLESTRIP &&
        type!=D3DPT_TRIANGLEFAN))return 0u;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return 0u;
    const auto layout=DecodeWorldUpFVF(fvf,stride);
    if(!layout.eligible)return 0u;
    if(family==4u && (!indices||!vertex_count||
       vertex_count>m2vr::drawprobe::kMaxIndexedUPDeclaredVertices ||
       (index_format!=D3DFMT_INDEX16 &&
        index_format!=D3DFMT_INDEX32)))return 0u;
    D3DMATRIX world{},view{};
    if(FAILED(dev->GetTransform(D3DTS_WORLD,&world)) ||
       FAILED(dev->GetTransform(D3DTS_VIEW,&view)) ||
       !AffineMatrix(world)||!AffineMatrix(view))return 0u;
    const auto elements=PrimitiveElements(
        static_cast<std::uint32_t>(type),primitive_count);
    if(!elements)return 0u;
    constexpr unsigned max_sample=3u;
    std::array<WorldVertex,max_sample> samples{};
    std::array<std::uint32_t,max_sample> ids{};
    const unsigned count=elements<max_sample?elements:max_sample;
    for(unsigned i=0;i<count;++i) {
        std::uint32_t index=i;
        std::uint32_t absolute_index=index;
        if(family==4u) {
            const unsigned bytes=index_format==D3DFMT_INDEX16?2u:4u;
            std::array<std::uint8_t,4> raw{};
            if(!SafeReadBytes(indices,std::size_t(i)*bytes,
                              raw.data(),bytes))return 0u;
            if(bytes==2u){std::uint16_t x=0;std::memcpy(&x,raw.data(),2);index=x;}
            else std::memcpy(&index,raw.data(),4);
            std::uint32_t relative=0u;
            if(!IndexedUPRelativeIndex(index,first,vertex_count,relative))
                return 0u;
            absolute_index=index;
            index=relative; // UP vertex pointer begins at first
        }
        // Read only fixed layout fields. Never dereference beyond 65536
        // source bytes, and never ask the caller to provide hidden memory.
        std::array<std::uint8_t,256> source{};
        const std::size_t vertex_base=std::size_t(index)*stride;
        if(!SafeReadBytes(vertices,vertex_base,source.data(),12u) ||
           (layout.has_diffuse &&
            !SafeReadBytes(vertices,vertex_base+layout.diffuse_offset,
                           source.data()+layout.diffuse_offset,4u)) ||
           (layout.has_uv &&
            !SafeReadBytes(vertices,vertex_base+layout.uv_offset,
                           source.data()+layout.uv_offset,8u)) ||
           !DecodeWorldVertex(source.data(),stride,layout,
                              world,view,samples[i]))return 0u;
        ids[i]=absolute_index;
    }
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    LogOriginalSourcePassContext(dev,id);
    LogOriginalFixedStageEvidence(dev,id,fvf,stride);
    // Source FVF, stride and upcoming original Present are pinned to
    // this SAME original world-UP sample, never inferred from other Draws.
    m2vr::LogEvent("model2_d3d9_up_source_fvf",id,fvf,stride,
        g_present_count.load(std::memory_order_relaxed)+1u);
    m2vr::LogEvent("model2_world_up_candidate",id,
         CallerRva(caller),fvf,
         (std::uint64_t{family}<<32)|stride);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_window",id,
            first,vertex_count,index_format);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_topology",id,
            static_cast<unsigned int>(type),primitive_count,
            g_present_count.load(std::memory_order_relaxed)+1u);
    for(unsigned i=0;i<count;++i){
        const auto& v=samples[i];
        m2vr::LogEvent("model2_world_up_view_xyz",id,ids[i],
            PackPair(v.view_xyz[0],v.view_xyz[1]),
            PackPair(v.view_xyz[2],v.uv[0]));
        m2vr::LogEvent("model2_world_up_source_uv",id,ids[i],
            PackPair(v.uv[0],v.uv[1]),0u);
    }
    IDirect3DBaseTexture9* tex=nullptr;
    if(SUCCEEDED(dev->GetTexture(0,&tex)) && tex) {
        if(tex->GetType()==D3DRTYPE_TEXTURE) {
            auto* t=static_cast<IDirect3DTexture9*>(tex);
            D3DSURFACE_DESC desc{};
            if(SUCCEEDED(t->GetLevelDesc(0,&desc)))
                m2vr::LogEvent("model2_world_up_texture_meta",id,
                    desc.Format,desc.Width,desc.Height);
        }
        tex->Release();
    }
    return id;
}
// Candidate camera fit is implemented after ObserveUP below.
void ObserveProjectionFit(IDirect3DDevice9* dev,std::uint64_t witness,
        const m2vr::drawprobe::ProjectedTriangleBatchJoin& batch);
// Defined below: bounded original FVF=0 declaration observation.
std::uint64_t ObserveUnknownUpLayout(IDirect3DDevice9* dev,unsigned family,
                            void* caller,UINT stride,
                            D3DPRIMITIVETYPE topology,
                            UINT primitive_count,const void* vertices);
std::uint64_t ObserveUP(IDirect3DDevice9* dev,unsigned family,
               D3DPRIMITIVETYPE type,UINT primitive_count,void* caller,
               const void* vertices,UINT stride,const void* indices=nullptr,
               D3DFORMAT index_format=D3DFMT_UNKNOWN,UINT first=0,
               UINT vertex_count=0) {
    using namespace m2vr::drawprobe;
    const auto elements=PrimitiveElements(
        static_cast<std::uint32_t>(type),primitive_count);
    if(!dev||!vertices||!ValidStride(stride)||!elements)return 0u;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return 0u;
    if(fvf==0u) {
        return ObserveUnknownUpLayout(dev,family,caller,stride,type,
                                      primitive_count,vertices);
    }
    if((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZ){
        return ObserveWorldUP(dev,family,type,primitive_count,caller,
                              vertices,stride,indices,index_format,first,vertex_count);
    }
    if((fvf&D3DFVF_POSITION_MASK)!=D3DFVF_XYZRHW)return 0u;

    // Nonzero MinVertexIndex is legal: resolve original absolute index
    // against the declared UP span before reading its local memory.
    if(family==4u && (!indices||!vertex_count||
       vertex_count>m2vr::drawprobe::kMaxIndexedUPDeclaredVertices ||
       (index_format!=D3DFMT_INDEX16 &&
        index_format!=D3DFMT_INDEX32)))return 0u;

    struct Observation {std::uint32_t index=0; VertexWitness vertex{};};
    // Capture the first eight original TRIANGLELIST, TRIANGLESTRIP or
    // TRIANGLEFAN triangles with source winding preserved, without another
    // original Draw call. Unknown topology stays at three source vertices.
    constexpr unsigned kJoinTriangles=
        static_cast<unsigned>(m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles);
    constexpr unsigned kJoinSampleVertices=3u*kJoinTriangles;
    std::array<Observation,kJoinSampleVertices> observations{};
    const bool triangle_topology=
        type==D3DPT_TRIANGLELIST ||
        type==D3DPT_TRIANGLESTRIP ||
        type==D3DPT_TRIANGLEFAN;
    const auto sampled_triangles=
        primitive_count<kJoinTriangles?primitive_count:kJoinTriangles;
    const auto triangle_elements=triangle_topology?
        PrimitiveSourceElementCount(type,sampled_triangles):0u;
    const unsigned limit=triangle_elements?triangle_elements:MaxVertices;
    const unsigned count=elements<limit?elements:limit;
    for(unsigned i=0;i<count;++i){
        std::uint32_t index=i;
        std::uint32_t absolute_index=index;
        if(family==4u){
            std::array<std::uint8_t,4> bytes{};
            const unsigned width=index_format==D3DFMT_INDEX16?2u:4u;
            if(!SafeReadBytes(indices,std::size_t{i}*width,
                              bytes.data(),width))return 0u;
            if(width==2u){
                std::uint16_t short_index=0;
                std::memcpy(&short_index,bytes.data(),2u);
                index=short_index;
            }else{
                std::memcpy(&index,bytes.data(),4u);
            }
            std::uint32_t relative=0u;
            if(!IndexedUPRelativeIndex(index,first,vertex_count,relative))
                return 0u;
            absolute_index=index;
            index=relative; // UP vertex pointer begins at first
        }
        std::array<std::uint32_t,4> bits{};
        if(!SafeReadBytes(vertices,std::size_t{index}*stride,
                          bits.data(),sizeof(bits)) ||
           !DecodeXYZRHW(bits,observations[i].vertex))return 0u;
        observations[i].index=absolute_index;
    }

    const auto sample_id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    LogOriginalSourcePassContext(dev,sample_id);
    LogOriginalFixedStageEvidence(dev,sample_id,fvf,stride);
    m2vr::LogEvent("model2_d3d9_up_source_fvf",sample_id,fvf,stride,
        g_present_count.load(std::memory_order_relaxed)+1u);
    m2vr::LogEvent("model2_d3d9_up_meta",sample_id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        CallerRva(caller),(std::uint64_t{family}<<32)|stride);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_window",sample_id,
            first,vertex_count,index_format);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_topology",sample_id,
            static_cast<unsigned int>(type),primitive_count,
            g_present_count.load(std::memory_order_relaxed)+1u);
    // Preserve the original three-vertex per-witness log volume/schema.
    // Extra source triangles are correlated only via aggregate metadata.
    for(unsigned i=0;i<count && i<MaxVertices;++i){
        const auto& v=observations[i].vertex;
        m2vr::LogEvent("model2_d3d9_up_vertex",sample_id,
            observations[i].index,PackPair(v.x,v.y),PackPair(v.z,v.rhw));
    }
    // The sampled original UP triangle can now be associated with
    // immediately preceding CPU projector outputs on this SAME thread and
    // Present epoch. Even a full match is a candidate, never world proof.
    if(triangle_elements && count==triangle_elements){
        std::vector<std::uint32_t> source_ids(count);
        for(unsigned i=0;i<count;++i)source_ids[i]=i;
        std::vector<std::uint32_t> triangle_ids;
        if(!ExpandOriginalTriangles(type,sampled_triangles,
                                    source_ids,triangle_ids))
            return sample_id;
        std::array<ProjectedDrawVertex,kJoinSampleVertices> screen{};
        for(std::size_t i=0;i<triangle_ids.size();++i){
            const auto& original=observations[triangle_ids[i]].vertex;
            screen[i]={original.x,original.y};
        }
        const auto epoch=CurrentProjectDrawEpoch();
        const auto& correlator=ThreadProjectDrawCorrelator();
        const auto join=correlator.Match(epoch,screen.data(),3u);
        m2vr::LogEvent("model2_d3d9_cpu_up_join",sample_id,
            join.matched,join.ambiguous,join.source_caller_rva);
        const auto batch=correlator.MatchTriangleList(
            epoch,screen.data(),triangle_ids.size());
        m2vr::LogEvent("model2_d3d9_cpu_up_join_batch",sample_id,
            (std::uint64_t{batch.inspected}<<32)|batch.complete,
            (std::uint64_t{batch.partial}<<32)|batch.ambiguous,
            batch.common_source_caller_rva);
        m2vr::LogEvent("model2_d3d9_cpu_up_matrix_identity",sample_id,
            batch.common_matrix_identity,
            batch.mixed_matrix_identity?1u:0u,
            batch.common_source_caller_rva);
        m2vr::LogEvent("model2_d3d9_cpu_up_matrix_content",sample_id,
            batch.common_matrix_fingerprint,
            batch.mixed_matrix_fingerprint?1u:0u,
            batch.common_matrix_identity);
        m2vr::LogEvent("model2_d3d9_cpu_up_geometry_command",sample_id,
            batch.common_geometry_command,
            batch.mixed_geometry_command?1u:0u,
            batch.common_matrix_fingerprint);
        // Evidence-only. A fitted original world camera remains unknown
        // until separately authenticated by private, real gameplay proof.
        ObserveProjectionFit(dev,sample_id,batch);
    }
    return sample_id;
}

// No original D3D9 draw mutation. A candidate affine perspective
// relationship between SAME source caller's source-XYZ and original screen
// coordinates provides camera-topology evidence but never world proof.
void ObserveProjectionFit(IDirect3DDevice9* dev,std::uint64_t witness,
        const m2vr::drawprobe::ProjectedTriangleBatchJoin& batch){
    if(!dev || !batch.CompleteSameSource())return;
    D3DVIEWPORT9 vp{};
    if(FAILED(dev->GetViewport(&vp)))return;
    const auto fit=m2vr::drawprobe::FitSourcePerspective(
        batch.camera_samples.data(),batch.camera_sample_count,
        vp.X,vp.Y,vp.Width,vp.Height);
    if(!fit.plausible)return;
    m2vr::LogEvent("model2_d3d9_cpu_projection_fit",witness,
        batch.common_source_caller_rva,
        m2vr::drawprobe::PackPair(fit.center_x,fit.center_y),
        m2vr::drawprobe::PackPair(fit.scale_x,fit.scale_y));
    m2vr::LogEvent("model2_d3d9_cpu_projection_fit_quality",witness,
        fit.accepted_samples,
        m2vr::drawprobe::PackPair(fit.rms_pixels,
            fit.depth_positive?1.f:-1.f),0u);
}

// Observe the original stream-zero D3D9 vertex declaration when GetFVF
// reports 0 (shader/declaration path). The actual Daytona world candidate
// uses this path for >8k mid-frame samples. Never treat FVF=0 as world
// proof, never read unknown UP source bytes, and never mutate D3D9 state.
// A declaration query is bounded to 16 elements including D3DDECL_END.
std::uint64_t ObserveUnknownUpLayout(IDirect3DDevice9* dev,unsigned family,
                            void* caller,UINT stride,
                            D3DPRIMITIVETYPE topology,
                            UINT primitive_count,const void* vertices) {
    if(!dev || family<3u || family>4u)return 0u;
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    // 0=unknown/error, 1=shader absent, 2=shader present.
    std::uint32_t vs_status=0u,ps_status=0u;
    IDirect3DVertexShader9* vs=nullptr;
    if(SUCCEEDED(dev->GetVertexShader(&vs)))
        vs_status=vs?2u:1u;
    if(vs)vs->Release();
    IDirect3DPixelShader9* ps=nullptr;
    if(SUCCEEDED(dev->GetPixelShader(&ps)))
        ps_status=ps?2u:1u;
    if(ps)ps->Release();
    IDirect3DVertexDeclaration9* declaration=nullptr;
    const HRESULT hr=dev->GetVertexDeclaration(&declaration);
    std::array<D3DVERTEXELEMENT9,16> elements{};
    UINT count=0u;
    if(SUCCEEDED(hr)&&declaration){
        count=static_cast<UINT>(elements.size());
        if(FAILED(declaration->GetDeclaration(elements.data(),&count)) ||
           count==0u || count>elements.size() ||
           elements[count-1u].Stream!=0xffu)count=0u;
    }
    if(declaration)declaration->Release();
    LogOriginalSourcePassContext(dev,id);
    m2vr::LogEvent("model2_d3d9_up_layout",id,CallerRva(caller),
        (std::uint64_t{family}<<32)|stride,
        (std::uint64_t{vs_status}<<40)|
        (std::uint64_t{ps_status}<<32)|count);
    // Only the original declarative byte offsets and D3D9 semantics
    // are logged. No game vertex data or shader bytecode is copied.
    for(UINT k=0u;k<count;++k) {
        const auto& e=elements[k];
        m2vr::LogEvent("model2_d3d9_up_decl_element",id,k,
            (std::uint64_t{e.Stream}<<32)|e.Offset,
            (std::uint64_t{e.Type}<<24)|
            (std::uint64_t{e.Method}<<16)|
            (std::uint64_t{e.Usage}<<8)|e.UsageIndex);
    }
    // After v3 confirmed exactly this 32-byte declaration on *real*
    // Daytona DrawPrimitiveUP, safely witness only its first triangle.
    // FVF=0 alone is insufficient: reject any other declaration,
    // stride, topology, missing shaders or indexed-source mapping.
    // No GPU state is changed, and source vertices are never replayed.
    if(family!=3u || !vertices || topology!=D3DPT_TRIANGLELIST ||
       !primitive_count || vs_status!=2u || ps_status!=2u ||
       !m2vr::drawprobe::ExactDaytonaUPDeclaration(
           elements.data(),count,stride))return id;
    std::array<m2vr::drawprobe::DaytonaUPVertex,3> samples{};
    for(unsigned i=0;i<3u;++i) {
        std::array<std::uint8_t,32> bytes{};
        const std::size_t source_offset=std::size_t{i}*stride;
        if(!SafeReadBytes(vertices,source_offset,bytes.data(),16u) ||
           !SafeReadBytes(vertices,source_offset+24u,
                          bytes.data()+24u,8u) ||
           !m2vr::drawprobe::DecodeDaytonaUPVertex(bytes,samples[i]))
            return id;
    }
    m2vr::LogEvent("model2_d3d9_up_position_source",id,
        CallerRva(caller),primitive_count,3u);
    // Bind the known shader-UP witness to the original Present frame.
    m2vr::LogEvent("model2_d3d9_up_layout_frame",id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        CallerRva(caller),0u);
    for(unsigned i=0;i<3u;++i) {
        const auto& v=samples[i];
        m2vr::LogEvent("model2_d3d9_up_position",id,i,
            m2vr::drawprobe::PackPair(v.position[0],v.position[1]),
            m2vr::drawprobe::PackPair(v.position[2],v.position[3]));
        m2vr::LogEvent("model2_d3d9_up_uv",id,i,
            m2vr::drawprobe::PackPair(v.texcoord[0],v.texcoord[1]),0u);
    }
    // Hypothesis only: evaluate the original 32-byte shader-UP POSITION0
    // as clip-space, compare its viewport pixels to native CPU projector
    // witnesses from the same thread/Present epoch. Unknown shader WVP
    // must never be promoted to a calibrated 6DoF game camera.
    D3DVIEWPORT9 vp{};
    if(SUCCEEDED(dev->GetViewport(&vp)) &&
       vp.MinZ==0.f && vp.MaxZ==1.f){
        std::array<m2vr::drawprobe::ProjectedDrawVertex,3> screen{};
        bool candidate=true;
        for(unsigned i=0;i<3u;++i){
            if(!m2vr::drawprobe::CandidateClipToViewport(
                   samples[i].position.data(),vp.X,vp.Y,
                   vp.Width,vp.Height,screen[i])){
                candidate=false;break;
            }
        }
        if(candidate){
            const auto join=m2vr::drawprobe::ThreadProjectDrawCorrelator().Match(
                m2vr::drawprobe::CurrentProjectDrawEpoch(),
                screen.data(),screen.size());
            m2vr::LogEvent("model2_d3d9_cpu_shader_up_join",id,
                join.matched,join.ambiguous,join.source_caller_rva);
            // The earlier witness stopped after the first triangle even
            // for hundreds of original shader-UP world candidates. Sample
            // up to seven more, never read shader bytecode or change state.
            constexpr unsigned max_vertices=
                3u*m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles;
            std::array<m2vr::drawprobe::ProjectedDrawVertex,max_vertices>
                batch_vertices{};
            for(unsigned i=0;i<3u;++i)batch_vertices[i]=screen[i];
            unsigned valid_triangles=1u;
            const unsigned limit=primitive_count<
                m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles?
                primitive_count:static_cast<unsigned>(
                m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles);
            for(unsigned tri=1u;tri<limit;++tri){
                bool valid=true;
                for(unsigned j=0u;j<3u;++j){
                    const std::size_t vertex_index=std::size_t(tri)*3u+j;
                    std::array<std::uint8_t,32> bytes{};
                    m2vr::drawprobe::DaytonaUPVertex vertex{};
                    if(!SafeReadBytes(vertices,vertex_index*stride,
                                      bytes.data(),16u) ||
                       !SafeReadBytes(vertices,vertex_index*stride+24u,
                                      bytes.data()+24u,8u) ||
                       !m2vr::drawprobe::DecodeDaytonaUPVertex(bytes,vertex) ||
                       !m2vr::drawprobe::CandidateClipToViewport(
                           vertex.position.data(),vp.X,vp.Y,
                           vp.Width,vp.Height,
                           batch_vertices[vertex_index])){
                        valid=false;break;
                    }
                }
                if(!valid)break; // never count a partial triangle
                ++valid_triangles;
            }
            const auto batch=
                m2vr::drawprobe::ThreadProjectDrawCorrelator()
                    .MatchTriangleList(
                        m2vr::drawprobe::CurrentProjectDrawEpoch(),
                        batch_vertices.data(),
                        std::size_t(valid_triangles)*3u);
            m2vr::LogEvent("model2_d3d9_cpu_shader_up_join_batch",id,
                (std::uint64_t{batch.inspected}<<32)|batch.complete,
                (std::uint64_t{batch.partial}<<32)|batch.ambiguous,
                batch.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_shader_matrix_identity",id,
                batch.common_matrix_identity,
                batch.mixed_matrix_identity?1u:0u,
                batch.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_shader_matrix_content",id,
                batch.common_matrix_fingerprint,
                batch.mixed_matrix_fingerprint?1u:0u,
                batch.common_matrix_identity);
            m2vr::LogEvent("model2_d3d9_cpu_shader_geometry_command",id,
                batch.common_geometry_command,
                batch.mixed_geometry_command?1u:0u,
                batch.common_matrix_fingerprint);
            ObserveProjectionFit(dev,id,batch);
        }
    }
    return id;
}

// Source-only D3D9 DrawPrimitive/DrawIndexedPrimitive provenance.
// No buffer Lock(), texture copy, original draw replay or world-pass flag.
// The actual real game source ranges and fixed-function camera state are
// associated by witness ID for offline, authorized render-path analysis.
std::uint64_t ObserveBufferSource(IDirect3DDevice9* dev,unsigned family,
                         D3DPRIMITIVETYPE type,void* caller,
                         UINT start_vertex,INT base_vertex,
                         UINT min_vertex,UINT num_vertices,
                         UINT start_index,UINT primitive_count) {
    using namespace m2vr::drawprobe;
    if(!dev || (family!=1u && family!=2u) ||
       !m2vr::drawprobe::PrimitiveSourceElementCount(
           type,primitive_count))return 0u;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return 0u;
    IDirect3DVertexBuffer9* vb=nullptr;
    UINT offset=0,stride=0;
    if(FAILED(dev->GetStreamSource(0,&vb,&offset,&stride)) || !vb)
        return 0u;
    D3DVERTEXBUFFER_DESC vdesc{};
    const bool desc_ok=SUCCEEDED(vb->GetDesc(&vdesc));
    vb->Release();
    if(!desc_ok)return 0u;
    const auto layout=DecodeWorldUpFVF(fvf,stride);
    const bool projected=ExactProjectedUPWorldFVF(fvf,stride);
    if(!layout.eligible && !projected)return 0u; // no guessed declarations
    D3DMATRIX world{},view{};
    const bool transforms_ok=
        SUCCEEDED(dev->GetTransform(D3DTS_WORLD,&world)) &&
        SUCCEEDED(dev->GetTransform(D3DTS_VIEW,&view)) &&
        AffineMatrix(world)&&AffineMatrix(view);
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    // Exact upcoming original main-device Present, never simulation tick.
    // This distinguishes independent scene passes from repeated calls
    // within one frame when validating original world/HUD ownership.
    LogOriginalSourcePassContext(dev,id);
    LogOriginalFixedStageEvidence(dev,id,fvf,stride);
    m2vr::LogEvent("model2_d3d9_vb_present_frame",id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        family,0u);
    m2vr::LogEvent("model2_d3d9_vb_source",id,CallerRva(caller),
        (std::uint64_t{family}<<32)|fvf,
        (std::uint64_t{vdesc.Size}<<32)|stride);
    m2vr::LogEvent("model2_d3d9_vb_stream",id,
        (std::uint64_t{offset}<<32)|vdesc.Usage,
        vdesc.Pool,projected?16u:layout.uv_offset);
    if(projected){
        // The source XYZRHW screen position can be matched against this
        // thread's original CPU projected points, unlike raw XYZ stream.
        // Still only candidate evidence: never authorizes stereo rendering.
        ProjectedTriangleBatchJoin joined{};
        if(InspectProjectedD3D9VBTopologyJoin(dev,family==2u,type,
                start_vertex,base_vertex,min_vertex,num_vertices,
                start_index,primitive_count,joined)){
            m2vr::LogEvent("model2_d3d9_cpu_vb_join_batch",id,
                (std::uint64_t{joined.inspected}<<32)|joined.complete,
                (std::uint64_t{joined.partial}<<32)|joined.ambiguous,
                joined.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_vb_matrix_identity",id,
                joined.common_matrix_identity,
                joined.mixed_matrix_identity?1u:0u,
                joined.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_vb_matrix_content",id,
                joined.common_matrix_fingerprint,
                joined.mixed_matrix_fingerprint?1u:0u,
                joined.common_matrix_identity);
            m2vr::LogEvent("model2_d3d9_cpu_vb_geometry_command",id,
                joined.common_geometry_command,
                joined.mixed_geometry_command?1u:0u,
                joined.common_matrix_fingerprint);
            ObserveProjectionFit(dev,id,joined);
        }
    }
    if(family==1u){
        m2vr::LogEvent("model2_d3d9_vb_draw_args",id,family,
            start_vertex,primitive_count);
    }else{
        m2vr::LogEvent("model2_d3d9_vb_draw_args",id,
            static_cast<std::uint32_t>(base_vertex),
            (std::uint64_t{min_vertex}<<32)|num_vertices,
            (std::uint64_t{start_index}<<32)|primitive_count);
        IDirect3DIndexBuffer9* ib=nullptr;
        if(SUCCEEDED(dev->GetIndices(&ib))&&ib){
            D3DINDEXBUFFER_DESC idesc{};
            if(SUCCEEDED(ib->GetDesc(&idesc)))
                m2vr::LogEvent("model2_d3d9_ib_source",id,
                    idesc.Format,idesc.Size,idesc.Usage);
            ib->Release();
        }
    }
    if(transforms_ok && !projected){
        m2vr::LogEvent("model2_d3d9_vb_camera",id,
            PackPair(world._41,world._42),
            PackPair(world._43,view._41),
            PackPair(view._42,view._43));
    }else{
        m2vr::LogEvent("model2_d3d9_vb_camera_unknown",id);
    }
    // Evidence-only snapshot of the sampler/alpha modes that the native
    // stereo material adapter can (or cannot) currently represent.
    // Unknown states remain unknown, never reported as supported.
    DWORD cull=0,zfunc=0,zwrite=0,stencil=0,depth_bias=0,slope_bias=0;
    DWORD srgb_write=0,color_mask=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE,&cull)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ZFUNC,&zfunc)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_STENCILENABLE,&stencil)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_DEPTHBIAS,&depth_bias)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&slope_bias)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_SRGBWRITEENABLE,&srgb_write)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_COLORWRITEENABLE,&color_mask))) {
        m2vr::LogEvent("model2_d3d9_vb_raster",id,
            (std::uint64_t{cull}<<32)|zfunc,
            (std::uint64_t{zwrite}<<32)|stencil,
            (std::uint64_t{depth_bias}<<32)|slope_bias);
        m2vr::LogEvent("model2_d3d9_vb_write_color",id,
            (std::uint64_t{srgb_write}<<32)|color_mask,0u,0u);
    }
    DWORD au=0,av=0,mn=0,mg=0,mip=0,srgb=0;
    if(SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_ADDRESSU,&au)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_ADDRESSV,&av)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MINFILTER,&mn)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MAGFILTER,&mg)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MIPFILTER,&mip)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb))) {
        m2vr::LogEvent("model2_d3d9_vb_sampler",id,
            (std::uint64_t{au}<<32)|av,
            (std::uint64_t{mn}<<32)|mg,
            (std::uint64_t{mip}<<32)|srgb);
    }
    DWORD ae=0,af=0,ar=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHATESTENABLE,&ae)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAFUNC,&af)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAREF,&ar))) {
        m2vr::LogEvent("model2_d3d9_vb_alpha",id,
            (std::uint64_t{ae}<<32)|af,ar,0u);
    }
    IDirect3DBaseTexture9* base=nullptr;
    if(SUCCEEDED(dev->GetTexture(0,&base))&&base){
        if(base->GetType()==D3DRTYPE_TEXTURE){
            auto* tex=static_cast<IDirect3DTexture9*>(base);
            D3DSURFACE_DESC tdesc{};
            if(SUCCEEDED(tex->GetLevelDesc(0,&tdesc)))
                m2vr::LogEvent("model2_d3d9_vb_texture",id,
                    tdesc.Format,tdesc.Width,tdesc.Height);
        }
        base->Release();
    }
    return id;
}

void RecordOriginalSourceCompletion(std::uint64_t witness,
     std::uint64_t frame,std::uint64_t ordinal,
     m2vr::drawprobe::OriginalPhysicalSampleCompletion evidence) {
    if(!witness)return;
    // A single genuine hardware Draw COM completion, never a guessed
    // pre-COM Scene/RT snapshot or CPU projection-only correlation.
    m2vr::LogEvent("model2_d3d9_source_draw_completion",witness,
        frame,ordinal,evidence.Encode());
}
void RecordOriginalSourceResult(std::uint64_t witness,HRESULT hr,
                                void* caller,unsigned family,
                                std::uint64_t original_frame,
                                std::uint64_t frame_ordinal,
                                unsigned scene_ordinal,bool scene_open,
                                const m2vr::drawprobe::OriginalLineageStamp& lineage) {
    if(!witness)return;
    // Capture phase BEFORE the original COM Draw; a concurrent Present
    // must not relabel the returned HRESULT with the next frame.
    m2vr::LogEvent("model2_d3d9_source_draw_phase",witness,
        original_frame,frame_ordinal,
        (std::uint64_t{CallerRva(caller)}<<32)|family);
    // Only an installed *pair* of scene boundary hooks can assert the
    // original device's scene epoch. Failed Begin/End or unknown hooks
    // never silently qualify a same-scene source camera candidate.
    if(g_original_scene_hooks_ready.load(std::memory_order_acquire))
        m2vr::LogEvent("model2_d3d9_source_scene_phase",witness,
            original_frame,scene_ordinal,scene_open?1u:0u);
    m2vr::LogEvent(family<=2u?"model2_d3d9_vb_original_result":
                               "model2_d3d9_up_original_result",witness,
        static_cast<std::uint32_t>(hr),CallerRva(caller),
        (original_frame<<32)|family);
    // Exact witness join: never repurpose the original source camera
    // state from a later physical Present or unrelated Draw/scene.
    if(lineage.physical_present==original_frame &&
       m2vr::drawprobe::OriginalLineageUsable(lineage)){
        const auto& w=lineage.successful_writes;
        m2vr::LogEvent("model2_d3d9_source_camera_lineage",witness,
            original_frame,(std::uint64_t{w[0]}<<32)|w[1],
            (std::uint64_t{w[2]}<<32)|w[3]);
        m2vr::LogEvent("model2_d3d9_source_region_lineage",witness,
            original_frame,(std::uint64_t{w[4]}<<32)|w[5],
            (std::uint64_t{w[6]}<<32)|w[7]);
        // The real same-Draw source's buffers/layout/shaders, never pointers
        // or shader bytes, have bounded source-revision witnesses too.
        m2vr::LogEvent("model2_d3d9_source_geometry_lineage",witness,
            original_frame,(std::uint64_t{w[8]}<<32)|w[9],
            (std::uint64_t{w[10]}<<32)|w[11]);
        m2vr::LogEvent("model2_d3d9_source_shader_lineage",witness,
            original_frame,w[12],w[13]);
        // Same real Draw material revisions, never texture/constant bytes.
        m2vr::LogEvent("model2_d3d9_source_material_lineage",witness,
            original_frame,(std::uint64_t{w[14]}<<32)|w[15],
            (std::uint64_t{w[16]}<<32)|w[17]);
        m2vr::LogEvent("model2_d3d9_source_render_lineage",witness,
            original_frame,w[18],0u);
        // Apply bypasses the ordinary per-state COM setters. Track its
        // revision separately; unknown state contents never grant World3D.
        m2vr::LogEvent("model2_d3d9_source_stateblock_lineage",witness,
            original_frame,w[19],0u);
        m2vr::LogEvent("model2_d3d9_source_shader_aux_lineage",witness,
            original_frame,(std::uint64_t{w[20]}<<32)|w[21],
            (std::uint64_t{w[22]}<<32)|w[23]);
        m2vr::LogEvent("model2_d3d9_source_stream_frequency_lineage",witness,
            original_frame,w[24],0u);
        // Only original state revision counters; no light, material or plane data.
        m2vr::LogEvent("model2_d3d9_source_fixed_function_lineage",witness,
            original_frame,(std::uint64_t{w[25]}<<32)|w[26],
            (std::uint64_t{w[27]}<<32)|w[28]);
        m2vr::LogEvent("model2_d3d9_source_texture_upload_lineage",witness,
            original_frame,w[29],w[40]);
        m2vr::LogEvent("model2_d3d9_source_texture_shape_lineage",witness,
            original_frame,(std::uint64_t{w[41]}<<32)|w[42],w[43]);
        m2vr::LogEvent("model2_d3d9_source_texture_mip_lineage",witness,
            original_frame,(std::uint64_t{w[44]}<<32)|w[45],w[46]);
        m2vr::LogEvent("model2_d3d9_source_texture_lod_lineage",witness,
            original_frame,w[47],0u);
        m2vr::LogEvent("model2_d3d9_source_gpu_surface_lineage",witness,
            original_frame,w[48],0u);
        m2vr::LogEvent("model2_d3d9_source_palette_lineage",witness,
            original_frame,(std::uint64_t{w[30]}<<32)|w[31],0u);
        m2vr::LogEvent("model2_d3d9_source_raster_mode_lineage",witness,
            original_frame,(std::uint64_t{w[32]}<<32)|w[33],w[34]);
        m2vr::LogEvent("model2_d3d9_source_buffer_content_lineage",witness,
            original_frame,(std::uint64_t{w[38]}<<32)|w[39],0u);
    }
    g_original_source_result_samples.fetch_add(1u,std::memory_order_relaxed);
}
// Only a separately authenticated, one-use rule may forward a physical
// original primary-device EXE Draw to the already existing CPU-world raster.
// The actual emulator COM Draw and steering/FFB are never replayed here.
template<class Submit>
bool ForwardAuthenticatedOriginalDraw(IDirect3DDevice9* device,
    std::uint64_t frame,std::uint64_t ordinal,unsigned family,
    void* caller,unsigned scene,bool original_scene_open,
    const m2vr::drawprobe::OriginalLineageStamp& lineage,Submit&& submit){
    if(!g_original_target_draw_coverage.CanAttemptVerifiedWorldDraw() ||
       !g_original_camera_lifetime.MayForwardCpuWorld() ||
       g_state_block_apply_unobserved.load(std::memory_order_acquire) ||
       g_original_stream_frequency_state.UnsupportedMask() ||
       g_original_raster_mode_state.UnsupportedMask() ||
       g_buffer_hook_unknown.load(std::memory_order_acquire) ||
       g_original_bound_write_locks.HasOutstandingWrites() ||
       g_texture_hook_unknown.load(std::memory_order_acquire) ||
       g_original_texture_write_locks.HasOutstandingWrites())
        return false; // unknown original resource bytes are not stereo proof
    m2vr::vrbridge::ArmedOriginalDraw permission{};
    if(!m2vr::vrbridge::TryTakeAuthenticatedOriginalDraw(
          frame,ordinal,family,CallerRva(caller),scene,
          original_scene_open && g_original_scene_hooks_ready.load(
              std::memory_order_acquire),
          m2vr::drawprobe::OriginalLineageNonce(lineage),permission))return false;
    const BOOL rendered=submit(permission.cpu_projector,
                               permission.alpha_cutoff);
    // Append exports already reject their own invalid materials. Explicitly
    // revoke if the adapter failed before it could own any eye pixels.
    if(!rendered)Model2VR_RejectVerifiedD3D9SourceDraw(family);
    return rendered!=FALSE;
}
HRESULT STDMETHODCALLTYPE DrawHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT start,UINT count){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw(d,p,start,count),1u,caller,d);
    g_draws.fetch_add(1,std::memory_order_relaxed);
    const auto source_operation=g_original_scene_draw_coverage.EnterOperation();
    EnsurePhysicalOriginalRtDepth(d);
    const auto original_target_ticket=g_original_target_draw_coverage.EnterDraw();
    const auto camera_draw_ticket=g_original_camera_lifetime.EnterDraw();
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    ObserveOriginalAllFamilyDrawCaller(caller,source_frame,source_ordinal,1u);
    const bool startup_first_draw=TraceFirstOriginalDrawEntry(1u,source_frame);
    const auto source_scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    const bool source_scene_open=g_original_scene_open.load(
        std::memory_order_relaxed) &&
        g_original_scene_owner_thread.load(std::memory_order_relaxed)==
        GetCurrentThreadId();
    const std::uint64_t witness=Sample(d,1,p,count,caller)?
        ObserveBufferSource(d,1,p,caller,start,0,0,0,0,count):0u;
    const auto original_lineage=ReadOriginalLineage(source_frame);
    const bool forwarded=ForwardAuthenticatedOriginalDraw(d,source_frame,source_ordinal,
        1u,caller,
        source_scene,source_scene_open,original_lineage,[&](unsigned cpu,unsigned alpha){
        return cpu
            ?Model2VR_AppendVerifiedProjectedD3D9VBTopologyTextured(
                d,static_cast<unsigned>(p),start,count,cpu,alpha)
            :Model2VR_AppendVerifiedD3D9VBTopologyTextured(
                d,static_cast<unsigned>(p),start,count,alpha);
    });
    TraceFirstOriginalDrawStage(startup_first_draw,2u,1u,source_frame);
    const HRESULT hr=g_draw(d,p,start,count);
    TraceFirstOriginalDrawStage(startup_first_draw,3u,1u,source_frame,
        static_cast<std::uint32_t>(hr));
    // Source state must still be identical AFTER the authentic COM Draw;
    // a concurrent EXE state writer must not publish stale CPU/XR eyes.
    const bool camera_retained=g_original_camera_lifetime.CompleteDraw(
        camera_draw_ticket);
    const bool same_lineage=camera_retained &&
        m2vr::drawprobe::SameOriginalLineage(
            original_lineage,ReadOriginalLineage(source_frame));
    if(!camera_retained)m2vr::daytona::MarkOriginalLineageMismatch();
    if(forwarded && !same_lineage)
        Model2VR_RejectOriginalD3D9SourceLineageDrift(
            source_frame,source_ordinal,1u,CallerRva(caller));
    const bool target_retained=g_original_target_draw_coverage.CompleteDraw(
        original_target_ticket,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const bool scene_retained=OriginalSceneStillOwnedAfterDraw(
        source_scene,source_scene_open);
    g_original_scene_draw_coverage.Draw(source_operation,CallerRva(caller)!=0u,
        scene_retained,SUCCEEDED(hr));
    RecordOriginalSourceCompletion(witness,source_frame,source_ordinal,
        {SUCCEEDED(hr),scene_retained,target_retained,same_lineage});
    RecordOriginalSourceResult(witness,hr,caller,1u,
                               source_frame,source_ordinal,
                               source_scene,source_scene_open,original_lineage);
    TrackOriginalD3D9DrawResult(hr,1u,caller,d);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorPrimitive(d,p,start,count,g_draw);
    return hr;
}
HRESULT STDMETHODCALLTYPE IndexedHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,INT base,
                                      UINT min,UINT num,UINT start,UINT count){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw_indexed(d,p,base,min,num,start,count),2u,caller,d);
    g_indexed.fetch_add(1,std::memory_order_relaxed);
    const auto source_operation=g_original_scene_draw_coverage.EnterOperation();
    EnsurePhysicalOriginalRtDepth(d);
    const auto original_target_ticket=g_original_target_draw_coverage.EnterDraw();
    const auto camera_draw_ticket=g_original_camera_lifetime.EnterDraw();
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    ObserveOriginalAllFamilyDrawCaller(caller,source_frame,source_ordinal,2u);
    const bool startup_first_draw=TraceFirstOriginalDrawEntry(2u,source_frame);
    const auto source_scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    const bool source_scene_open=g_original_scene_open.load(
        std::memory_order_relaxed) &&
        g_original_scene_owner_thread.load(std::memory_order_relaxed)==
        GetCurrentThreadId();
    const std::uint64_t witness=Sample(d,2,p,count,caller)?
        ObserveBufferSource(d,2,p,caller,0,base,min,num,start,count):0u;
    const auto original_lineage=ReadOriginalLineage(source_frame);
    const bool forwarded=ForwardAuthenticatedOriginalDraw(d,source_frame,source_ordinal,
        2u,caller,
        source_scene,source_scene_open,original_lineage,[&](unsigned cpu,unsigned alpha){
        return cpu
            ?Model2VR_AppendVerifiedProjectedD3D9IndexedVBTopologyTextured(
                d,static_cast<unsigned>(p),base,min,num,start,count,cpu,alpha)
            :Model2VR_AppendVerifiedD3D9IndexedVBTopologyTextured(
                d,static_cast<unsigned>(p),base,min,num,start,count,alpha);
    });
    TraceFirstOriginalDrawStage(startup_first_draw,2u,2u,source_frame);
    const HRESULT hr=g_draw_indexed(d,p,base,min,num,start,count);
    TraceFirstOriginalDrawStage(startup_first_draw,3u,2u,source_frame,
        static_cast<std::uint32_t>(hr));
    // Source state must still be identical AFTER the authentic COM Draw;
    // a concurrent EXE state writer must not publish stale CPU/XR eyes.
    const bool camera_retained=g_original_camera_lifetime.CompleteDraw(
        camera_draw_ticket);
    const bool same_lineage=camera_retained &&
        m2vr::drawprobe::SameOriginalLineage(
            original_lineage,ReadOriginalLineage(source_frame));
    if(!camera_retained)m2vr::daytona::MarkOriginalLineageMismatch();
    if(forwarded && !same_lineage)
        Model2VR_RejectOriginalD3D9SourceLineageDrift(
            source_frame,source_ordinal,2u,CallerRva(caller));
    const bool target_retained=g_original_target_draw_coverage.CompleteDraw(
        original_target_ticket,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const bool scene_retained=OriginalSceneStillOwnedAfterDraw(
        source_scene,source_scene_open);
    g_original_scene_draw_coverage.Draw(source_operation,CallerRva(caller)!=0u,
        scene_retained,SUCCEEDED(hr));
    RecordOriginalSourceCompletion(witness,source_frame,source_ordinal,
        {SUCCEEDED(hr),scene_retained,target_retained,same_lineage});
    RecordOriginalSourceResult(witness,hr,caller,2u,
                               source_frame,source_ordinal,
                               source_scene,source_scene_open,original_lineage);
    TrackOriginalD3D9DrawResult(hr,2u,caller,d);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorIndexed(d,p,base,min,num,start,count,g_draw_indexed);
    return hr;
}
HRESULT STDMETHODCALLTYPE DrawUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT count,
                                    const void* verts,UINT stride){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw_up(d,p,count,verts,stride),3u,caller,d);
    g_up.fetch_add(1,std::memory_order_relaxed);
    const auto source_operation=g_original_scene_draw_coverage.EnterOperation();
    EnsurePhysicalOriginalRtDepth(d);
    const auto original_target_ticket=g_original_target_draw_coverage.EnterDraw();
    const auto camera_draw_ticket=g_original_camera_lifetime.EnterDraw();
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    ObserveOriginalAllFamilyDrawCaller(caller,source_frame,source_ordinal,3u);
    const bool startup_first_draw=TraceFirstOriginalDrawEntry(3u,source_frame);
    const auto source_scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    const bool source_scene_open=g_original_scene_open.load(
        std::memory_order_relaxed) &&
        g_original_scene_owner_thread.load(std::memory_order_relaxed)==
        GetCurrentThreadId();
    const std::uint64_t witness=Sample(d,3,p,count,caller)?
        ObserveUP(d,3,p,count,caller,verts,stride):0u;
    const auto original_lineage=ReadOriginalLineage(source_frame);
    const bool forwarded=ForwardAuthenticatedOriginalDraw(d,source_frame,source_ordinal,
        3u,caller,
        source_scene,source_scene_open,original_lineage,[&](unsigned cpu,unsigned alpha){
        return cpu
            ?Model2VR_AppendVerifiedProjectedD3D9PrimitiveUPTopologyTextured(
                d,static_cast<unsigned>(p),count,verts,stride,cpu,alpha)
            :Model2VR_AppendVerifiedD3D9PrimitiveUPTopologyTextured(
                d,static_cast<unsigned>(p),count,verts,stride,alpha);
    });
    TraceFirstOriginalDrawStage(startup_first_draw,2u,3u,source_frame);
    const HRESULT hr=g_draw_up(d,p,count,verts,stride);
    TraceFirstOriginalDrawStage(startup_first_draw,3u,3u,source_frame,
        static_cast<std::uint32_t>(hr));
    // Source state must still be identical AFTER the authentic COM Draw;
    // a concurrent EXE state writer must not publish stale CPU/XR eyes.
    const bool camera_retained=g_original_camera_lifetime.CompleteDraw(
        camera_draw_ticket);
    const bool same_lineage=camera_retained &&
        m2vr::drawprobe::SameOriginalLineage(
            original_lineage,ReadOriginalLineage(source_frame));
    if(!camera_retained)m2vr::daytona::MarkOriginalLineageMismatch();
    if(forwarded && !same_lineage)
        Model2VR_RejectOriginalD3D9SourceLineageDrift(
            source_frame,source_ordinal,3u,CallerRva(caller));
    const bool target_retained=g_original_target_draw_coverage.CompleteDraw(
        original_target_ticket,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const bool scene_retained=OriginalSceneStillOwnedAfterDraw(
        source_scene,source_scene_open);
    g_original_scene_draw_coverage.Draw(source_operation,CallerRva(caller)!=0u,
        scene_retained,SUCCEEDED(hr));
    RecordOriginalSourceCompletion(witness,source_frame,source_ordinal,
        {SUCCEEDED(hr),scene_retained,target_retained,same_lineage});
    RecordOriginalSourceResult(witness,hr,caller,3u,
                               source_frame,source_ordinal,
                               source_scene,source_scene_open,original_lineage);
    TrackOriginalD3D9DrawResult(hr,3u,caller,d);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorDraw(d,p,count,verts,stride,CallerRva(caller),g_draw_up);
    return hr;
}
HRESULT STDMETHODCALLTYPE IndexedUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT min,
     UINT num,UINT count,const void* idx,D3DFORMAT fmt,const void* verts,UINT stride){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride),4u,caller,d);
    g_indexed_up.fetch_add(1,std::memory_order_relaxed);
    const auto source_operation=g_original_scene_draw_coverage.EnterOperation();
    EnsurePhysicalOriginalRtDepth(d);
    const auto original_target_ticket=g_original_target_draw_coverage.EnterDraw();
    const auto camera_draw_ticket=g_original_camera_lifetime.EnterDraw();
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    ObserveOriginalAllFamilyDrawCaller(caller,source_frame,source_ordinal,4u);
    const bool startup_first_draw=TraceFirstOriginalDrawEntry(4u,source_frame);
    const auto source_scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    const bool source_scene_open=g_original_scene_open.load(
        std::memory_order_relaxed) &&
        g_original_scene_owner_thread.load(std::memory_order_relaxed)==
        GetCurrentThreadId();
    const std::uint64_t witness=Sample(d,4,p,count,caller)?
        ObserveUP(d,4,p,count,caller,verts,stride,idx,fmt,min,num):0u;
    const auto original_lineage=ReadOriginalLineage(source_frame);
    const bool forwarded=ForwardAuthenticatedOriginalDraw(d,source_frame,source_ordinal,
        4u,caller,
        source_scene,source_scene_open,original_lineage,[&](unsigned cpu,unsigned alpha){
        return cpu
            ?Model2VR_AppendVerifiedProjectedD3D9IndexedUPTopologyTextured(
                d,static_cast<unsigned>(p),min,num,count,idx,
                static_cast<unsigned>(fmt),verts,stride,cpu,alpha)
            :Model2VR_AppendVerifiedD3D9IndexedUPTopologyTextured(
                d,static_cast<unsigned>(p),min,num,count,idx,
                static_cast<unsigned>(fmt),verts,stride,alpha);
    });
    TraceFirstOriginalDrawStage(startup_first_draw,2u,4u,source_frame);
    const HRESULT hr=g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride);
    TraceFirstOriginalDrawStage(startup_first_draw,3u,4u,source_frame,
        static_cast<std::uint32_t>(hr));
    // Source state must still be identical AFTER the authentic COM Draw;
    // a concurrent EXE state writer must not publish stale CPU/XR eyes.
    const bool camera_retained=g_original_camera_lifetime.CompleteDraw(
        camera_draw_ticket);
    const bool same_lineage=camera_retained &&
        m2vr::drawprobe::SameOriginalLineage(
            original_lineage,ReadOriginalLineage(source_frame));
    if(!camera_retained)m2vr::daytona::MarkOriginalLineageMismatch();
    if(forwarded && !same_lineage)
        Model2VR_RejectOriginalD3D9SourceLineageDrift(
            source_frame,source_ordinal,4u,CallerRva(caller));
    const bool target_retained=g_original_target_draw_coverage.CompleteDraw(
        original_target_ticket,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const bool scene_retained=OriginalSceneStillOwnedAfterDraw(
        source_scene,source_scene_open);
    g_original_scene_draw_coverage.Draw(source_operation,CallerRva(caller)!=0u,
        scene_retained,SUCCEEDED(hr));
    RecordOriginalSourceCompletion(witness,source_frame,source_ordinal,
        {SUCCEEDED(hr),scene_retained,target_retained,same_lineage});
    RecordOriginalSourceResult(witness,hr,caller,4u,
                               source_frame,source_ordinal,
                               source_scene,source_scene_open,original_lineage);
    TrackOriginalD3D9DrawResult(hr,4u,caller,d);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorIndexedUP(d,p,min,num,count,idx,fmt,verts,stride,g_draw_indexed_up);
    return hr;
}
// Optional read-only scene boundary hooks: never add a scene or change
// an original COM HRESULT. A failed original scene call is not admitted.
HRESULT STDMETHODCALLTYPE BeginSceneHook(IDirect3DDevice9* d){
    void* const caller=_ReturnAddress();
    const bool primary=g_primary_device.Owns(d);
    const auto source_operation=primary?
        g_original_scene_draw_coverage.EnterOperation():0u;
    const HRESULT hr=g_begin_scene(d);
    // The game EXE must own the REAL source scene. Mirror/overlay DLL
    // transitions never assert original-scene provenance or poison it.
    const auto caller_rva=CallerRva(caller);
    if(primary && caller_rva){
        if(FAILED(hr))TrackFailedOriginalD3D9Scene(hr,1u,caller_rva);
        else if(!g_original_scene_poisoned.load(std::memory_order_acquire)){
            g_original_scene_ordinal.fetch_add(1u,std::memory_order_relaxed);
            g_original_scene_owner_thread.store(
                GetCurrentThreadId(),std::memory_order_release);
            g_original_scene_open.store(true,std::memory_order_release);
        }
        g_original_scene_draw_coverage.Begin(source_operation,
            true,SUCCEEDED(hr) &&
            !g_original_scene_poisoned.load(std::memory_order_acquire));
    }else if(primary){
        g_original_scene_draw_coverage.Begin(source_operation,false,false);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE EndSceneHook(IDirect3DDevice9* d){
    void* const caller=_ReturnAddress();
    const bool primary=g_primary_device.Owns(d);
    const auto source_operation=primary?
        g_original_scene_draw_coverage.EnterOperation():0u;
    const bool source_was_open=g_original_scene_open.load(
        std::memory_order_acquire) &&
        g_original_scene_owner_thread.load(std::memory_order_acquire)==
            GetCurrentThreadId();
    const HRESULT hr=g_end_scene(d);
    const auto caller_rva=CallerRva(caller);
    if(primary && caller_rva){
        if(FAILED(hr))TrackFailedOriginalD3D9Scene(hr,2u,caller_rva);
        else {
            g_original_scene_open.store(false,std::memory_order_release);
            g_original_scene_owner_thread.store(0u,std::memory_order_release);
        }
        g_original_scene_draw_coverage.End(source_operation,true,SUCCEEDED(hr),source_was_open);
    }else if(primary){
        g_original_scene_draw_coverage.End(source_operation,false,SUCCEEDED(hr),source_was_open);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE ClearHook(IDirect3DDevice9* d,DWORD count,
                                   const D3DRECT* rects,DWORD flags,
                                   D3DCOLOR color,float z,DWORD stencil){
    if(!g_primary_device.Owns(d))
        return g_clear(d,count,rects,flags,color,z,stencil);
    void* const caller=_ReturnAddress();
    const auto caller_rva=CallerRva(caller);
    BeforeOriginalGpuSurfaceWrite(caller_rva,1u);
    const HRESULT hr=g_clear(d,count,rects,flags,color,z,stencil);
    RecordOriginalGpuSurfaceWrite(caller_rva,hr,1u);
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Clear(hr,flags,caller_rva);
    else if(SUCCEEDED(hr)){
        LogOriginalD3D9MutationPhase(1u,caller_rva,hr);
        if(g_native_stereo)
            m2vr::daytona::MirrorClear(d,count,rects,flags,color,z,stencil,g_clear);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE UpdateSurfaceHook(IDirect3DDevice9* d,
     IDirect3DSurface9* src,const RECT* src_rect,IDirect3DSurface9* dst,
     const POINT* dst_point){
    if(!g_primary_device.Owns(d))return g_update_surface(d,src,src_rect,dst,dst_point);
    void* const caller=_ReturnAddress();
    const auto caller_rva=CallerRva(caller);
    BeforeOriginalGpuSurfaceWrite(caller_rva,2u);
    const HRESULT hr=g_update_surface(d,src,src_rect,dst,dst_point);
    RecordOriginalGpuSurfaceWrite(caller_rva,hr,2u);
    if(caller_rva && SUCCEEDED(hr))
        ObserveOriginalGpuSurfaceDestination(dst,caller_rva);
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,1u,caller_rva);
    else if(SUCCEEDED(hr)){
        LogOriginalD3D9MutationPhase(2u,caller_rva,hr);
        if(g_native_stereo)
            m2vr::daytona::MirrorUpdateSurface(
                d,src,src_rect,dst,dst_point,g_update_surface);
    }
    return hr;
}
// Real original UpdateTexture writes texels even without SetTexture. Preserve
// the game's actual COM HRESULT; only independent-eye provenance is tracked.
HRESULT STDMETHODCALLTYPE UpdateTextureHook(IDirect3DDevice9* d,
    IDirect3DBaseTexture9* source,IDirect3DBaseTexture9* destination){
    if(!g_primary_device.Owns(d))
        return g_update_texture(d,source,destination);
    void* const origin=_ReturnAddress();
    const auto caller_rva=CallerRva(origin);
    // Correctness repair: the original texture copy must not race an
    // already staged XR eye; invalidate BEFORE the native GPU transfer.
    if(caller_rva)Model2VR_ObserveOriginalD3D9TextureUpload(
        g_present_count.load(std::memory_order_acquire)+1u,caller_rva,
        g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    const HRESULT hr=g_update_texture(d,source,destination);
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,30u,caller_rva);
    if(caller_rva && SUCCEEDED(hr)){
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::TextureUpload,caller_rva,hr);
        LogOriginalD3D9MutationPhase(5u,caller_rva,hr);
    }
    return hr;
}
// A successful palette change may update already-bound texels without
// SetTexture or UpdateTexture. Original COM HRESULT and data are untouched.
HRESULT STDMETHODCALLTYPE SetPaletteEntriesHook(IDirect3DDevice9* d,
                                                UINT palette,const PALETTEENTRY* entries){
    if(!g_primary_device.Owns(d))return g_set_palette_entries(d,palette,entries);
    void* const origin=_ReturnAddress();
    const auto rva=CallerRva(origin);
    // The real COM method can overwrite an already-staged GPU/CPU eye's
    // indexed texels. Retire its original-Present source BEFORE that write.
    // A non-EXE caller on the primary device has unknown display ownership.
    if(rva)Model2VR_ObserveOriginalD3D9PaletteMutation(
        g_present_count.load(std::memory_order_acquire)+1u,rva,
        g_original_frame_draw_ordinal.load(std::memory_order_acquire),1u);
    else {
        g_original_external_presentation_unproven.store(
            true,std::memory_order_release);
        Model2VR_ObserveOriginalD3D9PresentationMutation(
            g_present_count.load(std::memory_order_acquire)+1u,0u,0u,5u);
    }
    const HRESULT hr=g_set_palette_entries(d,palette,entries);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,31u,rva);
    if(rva && SUCCEEDED(hr))
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::PaletteEntries,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetCurrentTexturePaletteHook(IDirect3DDevice9* d,
                                                        UINT palette){
    if(!g_primary_device.Owns(d))return g_set_current_texture_palette(d,palette);
    void* const origin=_ReturnAddress();
    const auto rva=CallerRva(origin);
    // Palette selection may change bound texture appearance without Draw,
    // even for an in-flight original stereo frame. Retire first.
    if(rva)Model2VR_ObserveOriginalD3D9PaletteMutation(
        g_present_count.load(std::memory_order_acquire)+1u,rva,
        g_original_frame_draw_ordinal.load(std::memory_order_acquire),2u);
    else {
        g_original_external_presentation_unproven.store(
            true,std::memory_order_release);
        Model2VR_ObserveOriginalD3D9PresentationMutation(
            g_present_count.load(std::memory_order_acquire)+1u,0u,0u,5u);
    }
    const HRESULT hr=g_set_current_texture_palette(d,palette);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,32u,rva);
    if(rva && SUCCEEDED(hr))
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::CurrentPalette,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE StretchRectHook(IDirect3DDevice9* d,
     IDirect3DSurface9* src,const RECT* src_rect,IDirect3DSurface9* dst,
     const RECT* dst_rect,D3DTEXTUREFILTERTYPE filter){
    if(!g_primary_device.Owns(d))return g_stretch_rect(d,src,src_rect,dst,dst_rect,filter);
    void* const caller=_ReturnAddress();
    const auto caller_rva=CallerRva(caller);
    BeforeOriginalGpuSurfaceWrite(caller_rva,3u);
    const HRESULT hr=g_stretch_rect(d,src,src_rect,dst,dst_rect,filter);
    RecordOriginalGpuSurfaceWrite(caller_rva,hr,3u);
    if(caller_rva && SUCCEEDED(hr))
        ObserveOriginalGpuSurfaceDestination(dst,caller_rva);
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,2u,caller_rva);
    else if(SUCCEEDED(hr)){
        LogOriginalD3D9MutationPhase(3u,caller_rva,hr);
        if(g_native_stereo)
            m2vr::daytona::MirrorStretchRect(
                d,src,src_rect,dst,dst_rect,filter,g_stretch_rect);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE ColorFillHook(IDirect3DDevice9* d,
     IDirect3DSurface9* dst,const RECT* rect,D3DCOLOR color){
    if(!g_primary_device.Owns(d))return g_color_fill(d,dst,rect,color);
    void* const caller=_ReturnAddress();
    const auto caller_rva=CallerRva(caller);
    BeforeOriginalGpuSurfaceWrite(caller_rva,4u);
    const HRESULT hr=g_color_fill(d,dst,rect,color);
    RecordOriginalGpuSurfaceWrite(caller_rva,hr,4u);
    if(caller_rva && SUCCEEDED(hr))
        ObserveOriginalGpuSurfaceDestination(dst,caller_rva);
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,3u,caller_rva);
    else if(SUCCEEDED(hr)){
        LogOriginalD3D9MutationPhase(4u,caller_rva,hr);
        if(g_native_stereo)
            m2vr::daytona::MirrorColorFill(d,dst,rect,color,g_color_fill);
    }
    return hr;
}
// Original target/depth COM calls are observed ONCE and never replayed.
// Failed authentic source bindings invalidate both speculative GPU eyes and
// authenticated CPU-world eyes for the same physical original Present.
HRESULT STDMETHODCALLTYPE SetRenderTargetHook(IDirect3DDevice9* d,DWORD slot,
                                              IDirect3DSurface9* surface){
    if(!g_primary_device.Owns(d))
        return g_set_render_target(d,slot,surface);
    void* const caller=_ReturnAddress();
    const auto target_bind=g_original_target_draw_coverage.EnterBind();
    const HRESULT hr=g_set_render_target(d,slot,surface);
    const auto caller_rva=CallerRva(caller);
    // Observe the real post-COM RT0/depth; never replay the game Set* call.
    g_original_target_draw_coverage.CompleteRenderTargetBind(
        target_bind,caller_rva!=0u,SUCCEEDED(hr),slot,surface!=nullptr,
        (SUCCEEDED(hr) && slot==0u)?ReadPhysicalOriginalRtDepth(d):
            m2vr::drawprobe::OriginalTargetSnapshot{});
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,4u,caller_rva);
    else if(SUCCEEDED(hr))
        LogOriginalD3D9TargetBindPhase(d,1u,caller_rva,slot,surface,hr);
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalLineageKind::RenderTarget,caller_rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetDepthStencilSurfaceHook(IDirect3DDevice9* d,
                                                     IDirect3DSurface9* surface){
    if(!g_primary_device.Owns(d))
        return g_set_depth_surface(d,surface);
    void* const caller=_ReturnAddress();
    const auto depth_bind=g_original_target_draw_coverage.EnterBind();
    const HRESULT hr=g_set_depth_surface(d,surface);
    const auto caller_rva=CallerRva(caller);
    g_original_target_draw_coverage.CompleteDepthBind(
        depth_bind,caller_rva!=0u,SUCCEEDED(hr),
        SUCCEEDED(hr)?ReadPhysicalOriginalRtDepth(d):
            m2vr::drawprobe::OriginalTargetSnapshot{});
    if(caller_rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,5u,caller_rva);
    else if(SUCCEEDED(hr))
        LogOriginalD3D9TargetBindPhase(d,2u,caller_rva,0u,surface,hr);
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalLineageKind::DepthSurface,caller_rva,hr);
    return hr;
}
// Real x86 original D3D9 transform provenance: informational only, never
// automatic Daytona World3D permission or OpenXR stereo authentication.
void RecordOriginalTransformOperation(D3DTRANSFORMSTATETYPE state,
    std::uint32_t rva,HRESULT hr,unsigned operation)noexcept{
    if(!rva || FAILED(hr) || (operation!=1u && operation!=2u))return;
    const auto kind=m2vr::drawprobe::ClassifyOriginalTransform(
        static_cast<std::uint32_t>(state));
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalTransformLineageKind(kind),rva,hr);
    const auto frame=g_present_count.load(std::memory_order_relaxed)+1u;
    if(frame!=1u && frame%8u!=0u)return;
    const auto ordinal=g_original_frame_draw_ordinal.load(
        std::memory_order_relaxed);
    const auto scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    if(ordinal>10000000u || scene>100000u)return;
    const auto category=static_cast<unsigned>(kind);
    // Preserve the legacy SetTransform CSV category 4="other" for old
    // PRIVATE traces. Internal nonce still differentiates texture/other.
    // Only newly introduced MultiplyTransform uses precise category 4/5.
    const auto observed_category=(operation==1u && category==5u)?
        4u:category;
    const auto slot=(operation-1u)*5u+observed_category-1u;
    if(g_original_transform_samples[slot].fetch_add(
           1u,std::memory_order_relaxed)!=0u)return;
    const bool open=g_original_scene_hooks_ready.load(
        std::memory_order_acquire) &&
        !g_original_scene_poisoned.load(std::memory_order_acquire) &&
        g_original_scene_open.load(std::memory_order_acquire) &&
        g_original_scene_owner_thread.load(std::memory_order_acquire)==
            GetCurrentThreadId();
    // Private CSV: physical Present, EXE RVA, bounded category/Draw and
    // scene/state flags ONLY. Never retain matrix values, pointers or pixels.
    m2vr::LogEvent(
        operation==1u?"model2_d3d9_original_transform_phase":
                      "model2_d3d9_original_multiply_transform_phase",
        frame,rva,(std::uint64_t{observed_category}<<32u)|ordinal,
        (std::uint64_t{scene}<<32u)|
        (std::uint64_t{static_cast<unsigned>(state)}<<1u)|
        (open?1u:0u));
}
HRESULT STDMETHODCALLTYPE SetTransformHook(IDirect3DDevice9* d,
    D3DTRANSFORMSTATETYPE state,const D3DMATRIX* matrix){
    if(!g_primary_device.Owns(d))return g_set_transform(d,state,matrix);
    void* const caller=_ReturnAddress();
    // Internal eye replay is not a genuine emulator source camera SET.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_transform(d,state,matrix);
    const auto camera_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_transform(d,state,matrix);
    g_original_camera_lifetime.CompleteWrite(
        camera_write,CallerRva(caller)!=0u,SUCCEEDED(hr)); // original COM once
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,6u,rva);
    RecordOriginalTransformOperation(state,rva,hr,1u);
    return hr;
}
// Genuine IDirect3DDevice9 COM 46 can alter view/world/texture state without
// invoking COM 44. Source lineage MUST reflect the actual resulting Draw.
HRESULT STDMETHODCALLTYPE MultiplyTransformHook(IDirect3DDevice9* d,
    D3DTRANSFORMSTATETYPE state,const D3DMATRIX* matrix){
    if(!g_primary_device.Owns(d))return g_multiply_transform(d,state,matrix);
    void* const caller=_ReturnAddress();
    // Internal eye replay is not a genuine emulator source camera SET.
    if(m2vr::daytona::IsEyeReplayActive())return g_multiply_transform(d,state,matrix);
    const auto camera_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_multiply_transform(d,state,matrix);
    g_original_camera_lifetime.CompleteWrite(
        camera_write,CallerRva(caller)!=0u,SUCCEEDED(hr)); // original COM once
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,36u,rva);
    RecordOriginalTransformOperation(state,rva,hr,2u);
    return hr;
}
// Observe actual original VS constant-write phase without copying the floats.
// A successful source write is not by itself a proven camera/World3D pass.
HRESULT STDMETHODCALLTYPE SetVSConstantFHook(IDirect3DDevice9* d,
    UINT start,const float* values,UINT count){
    if(!g_primary_device.Owns(d))
        return g_set_vs_constant_f(d,start,values,count);
    void* const caller=_ReturnAddress();
    // Internal eye replay is not a genuine emulator source camera SET.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_vs_constant_f(d,start,values,count);
    const auto camera_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_vs_constant_f(d,start,values,count);
    g_original_camera_lifetime.CompleteWrite(
        camera_write,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,7u,rva);
    if(!rva || FAILED(hr) || start>=256u || count==0u ||
       count>256u || start+count>256u)return hr;
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalLineageKind::VertexConstants,rva,hr);
    const auto frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto ordinal=g_original_frame_draw_ordinal.load(std::memory_order_relaxed);
    const auto scene=g_original_scene_ordinal.load(std::memory_order_relaxed);
    if((frame==1u || frame%8u==0u) && ordinal<=10000000u &&
       scene<=100000u &&
       g_original_vs_constant_samples.fetch_add(1u,std::memory_order_relaxed)==0u){
        const bool open=g_original_scene_hooks_ready.load(
            std::memory_order_acquire) &&
            !g_original_scene_poisoned.load(std::memory_order_acquire) &&
            g_original_scene_open.load(std::memory_order_acquire) &&
            g_original_scene_owner_thread.load(std::memory_order_acquire)==GetCurrentThreadId();
        m2vr::LogEvent("model2_d3d9_original_vs_constants_phase",
            frame,rva,(std::uint64_t{scene}<<32u)|ordinal,
            (std::uint64_t{start}<<32u)|(std::uint64_t{count}<<1u)|
            (open?1u:0u));
    }
    return hr;
}
// Resource9 COM 7/9 hooks are installed through the original VB/IB and
// texture/surface observers; the definition follows buffer hook helpers.
bool EnsureOriginalResourceResidencyObserved(IDirect3DResource9*,std::uint32_t);
bool EnsureOriginalResourcePrivateDataObserved(IDirect3DResource9*,std::uint32_t);
// Native original bound buffer Lock/Unlock hooks join one physical Present
// camera/geometry nonce and reject stale partial OpenXR stereo eye frames.
#include "original_d3d9_buffer_hooks.inl"
#include "original_d3d9_resource_residency_hooks.inl"
#include "original_d3d9_resource_private_data_hooks.inl"
#include "original_d3d9_container_hooks.inl"
#include "original_d3d9_texture_surface_hooks.inl"
#include "original_d3d9_texture_mip_hooks.inl"
#include "original_d3d9_texture_subresource_hooks.inl"
#include "original_d3d9_texture_shapes_hooks.inl"
#include "original_d3d9_texture_hooks.inl"
#include "original_d3d9_texture_create_hooks.inl"
#include "original_d3d9_buffer_create_hooks.inl"
#include "original_d3d9_surface_create_hooks.inl"
#include "original_d3d9_readback_hooks.inl"
#include "original_d3d9_surface_get_hooks.inl"
#include "original_d3d9_resource_get_hooks.inl"
#include "original_d3d9_swap_chain_hooks.inl"
#include "original_d3d9_query_hooks.inl"
#include "original_d3d9_validation_hooks.inl"
#include "original_d3d9_residency_hooks.inl"
#include "original_d3d9_presentation_hooks.inl"
#include "original_d3d9_cursor_hooks.inl"
// Primary EXE-owned original COM bindings execute once; their HRESULTs are
// preserved, while failures poison only speculative binocular output.
HRESULT STDMETHODCALLTYPE SetStreamSourceHook(IDirect3DDevice9* d,UINT stream,IDirect3DVertexBuffer9* buffer,UINT offset,UINT stride){
    if(!g_primary_device.Owns(d))return g_set_stream_source(d,stream,buffer,offset,stride);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_set_stream_source(d,stream,buffer,offset,stride);
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,10u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::StreamSource,rva,hr);
    if(rva && SUCCEEDED(hr) && buffer)
        ObserveOriginalBoundBuffer(buffer,1u,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetIndicesHook(IDirect3DDevice9* d,IDirect3DIndexBuffer9* buffer){
    if(!g_primary_device.Owns(d))return g_set_indices(d,buffer);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_set_indices(d,buffer);
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,11u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::IndexBuffer,rva,hr);
    if(rva && SUCCEEDED(hr) && buffer)
        ObserveOriginalBoundBuffer(buffer,2u,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetVertexDeclarationHook(IDirect3DDevice9* d,IDirect3DVertexDeclaration9* declaration){
    if(!g_primary_device.Owns(d))return g_set_vertex_declaration(d,declaration);
    void* const caller=_ReturnAddress();
    // Internal experimental mirrored eyes are NOT original game source
    // program/matrix writes, and must not taint real hardware Present.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_vertex_declaration(d,declaration);
    const auto source_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_vertex_declaration(d,declaration);
    g_original_camera_lifetime.CompleteWrite(
        source_write,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,12u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::VertexDeclaration,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetFVFHook(IDirect3DDevice9* d,DWORD fvf){
    if(!g_primary_device.Owns(d))return g_set_f_v_f(d,fvf);
    void* const caller=_ReturnAddress();
    // Internal experimental mirrored eyes are NOT original game source
    // program/matrix writes, and must not taint real hardware Present.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_f_v_f(d,fvf);
    const auto source_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_f_v_f(d,fvf);
    g_original_camera_lifetime.CompleteWrite(
        source_write,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,13u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::VertexFVF,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetVertexShaderHook(IDirect3DDevice9* d,IDirect3DVertexShader9* shader){
    if(!g_primary_device.Owns(d))return g_set_vertex_shader(d,shader);
    void* const caller=_ReturnAddress();
    // Internal eye replay is not a genuine emulator source camera SET.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_vertex_shader(d,shader);
    const auto camera_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_vertex_shader(d,shader);
    g_original_camera_lifetime.CompleteWrite(
        camera_write,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,14u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::VertexShader,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetPixelShaderHook(IDirect3DDevice9* d,IDirect3DPixelShader9* shader){
    if(!g_primary_device.Owns(d))return g_set_pixel_shader(d,shader);
    void* const caller=_ReturnAddress();
    // Internal experimental mirrored eyes are NOT original game source
    // program/matrix writes, and must not taint real hardware Present.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_pixel_shader(d,shader);
    const auto source_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_pixel_shader(d,shader);
    g_original_camera_lifetime.CompleteWrite(
        source_write,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,15u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::PixelShader,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetVSConstantIHook(IDirect3DDevice9* d,UINT start,const int* values,UINT count){
    if(!g_primary_device.Owns(d))return g_vs_i(d,start,values,count);
    void* const origin=_ReturnAddress();
    // Internal eye replay is not a genuine emulator source camera SET.
    if(m2vr::daytona::IsEyeReplayActive())return g_vs_i(d,start,values,count);
    const auto camera_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_vs_i(d,start,values,count);
    g_original_camera_lifetime.CompleteWrite(
        camera_write,CallerRva(origin)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,21u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::VertexConstantInt,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetVSConstantBHook(IDirect3DDevice9* d,UINT start,const BOOL* values,UINT count){
    if(!g_primary_device.Owns(d))return g_vs_b(d,start,values,count);
    void* const origin=_ReturnAddress();
    // Internal eye replay is not a genuine emulator source camera SET.
    if(m2vr::daytona::IsEyeReplayActive())return g_vs_b(d,start,values,count);
    const auto camera_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_vs_b(d,start,values,count);
    g_original_camera_lifetime.CompleteWrite(
        camera_write,CallerRva(origin)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,22u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::VertexConstantBool,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetPSConstantIHook(IDirect3DDevice9* d,UINT start,const int* values,UINT count){
    if(!g_primary_device.Owns(d))return g_ps_i(d,start,values,count);
    void* const origin=_ReturnAddress();
    // Internal experimental mirrored eyes are NOT original game source
    // program/matrix writes, and must not taint real hardware Present.
    if(m2vr::daytona::IsEyeReplayActive())return g_ps_i(d,start,values,count);
    const auto source_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_ps_i(d,start,values,count);
    g_original_camera_lifetime.CompleteWrite(
        source_write,CallerRva(origin)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,23u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::PixelConstantInt,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetPSConstantBHook(IDirect3DDevice9* d,UINT start,const BOOL* values,UINT count){
    if(!g_primary_device.Owns(d))return g_ps_b(d,start,values,count);
    void* const origin=_ReturnAddress();
    // Internal experimental mirrored eyes are NOT original game source
    // program/matrix writes, and must not taint real hardware Present.
    if(m2vr::daytona::IsEyeReplayActive())return g_ps_b(d,start,values,count);
    const auto source_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_ps_b(d,start,values,count);
    g_original_camera_lifetime.CompleteWrite(
        source_write,CallerRva(origin)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,24u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::PixelConstantBool,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetStreamFrequencyHook(IDirect3DDevice9* d,UINT stream,UINT frequency){
    if(!g_primary_device.Owns(d))return g_freq(d,stream,frequency);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_freq(d,stream,frequency);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,25u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::StreamFrequency,rva,hr);
    if(rva && SUCCEEDED(hr)){
        // The physical game's non-unit frequency can outlive a Present.
        // A later success Setting=1 only clears THIS stream.
        g_original_stream_frequency_state.SuccessfulOriginalWrite(
            stream,frequency);
        if(frequency!=1u)
            Model2VR_RejectOriginalD3D9InstancedStream(
                g_present_count.load(std::memory_order_acquire)+1u,
                stream,rva,frequency);
    }
    return hr;
}
// Original material/texture/pixel-constant writes; genuine COM/HRESULT
// unchanged. A failed EXE-owned mutation revokes both speculative eyes.
// These four native D3D9 COM observers forward the original method ONCE,
// retain HRESULT and leave original mono unaffected. Neither raw light,
// material nor clipping coefficients are captured or logged.
HRESULT STDMETHODCALLTYPE SetMaterialHook(IDirect3DDevice9* d,const D3DMATERIAL9* material){
    if(!g_primary_device.Owns(d))return g_set_material(d,material);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_set_material(d,material);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,26u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::Material,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetLightHook(IDirect3DDevice9* d,DWORD index,const D3DLIGHT9* light){
    if(!g_primary_device.Owns(d))return g_set_light(d,index,light);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_set_light(d,index,light);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,27u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::Light,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE LightEnableHook(IDirect3DDevice9* d,DWORD index,BOOL enabled){
    if(!g_primary_device.Owns(d))return g_light_enable(d,index,enabled);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_light_enable(d,index,enabled);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,28u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::LightEnable,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetClipPlaneHook(IDirect3DDevice9* d,DWORD index,const float* plane){
    if(!g_primary_device.Owns(d))return g_set_clip_plane(d,index,plane);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_set_clip_plane(d,index,plane);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,29u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::ClipPlane,rva,hr);
    return hr;
}
// Real original state calls, never duplicate game drawing or record game data.
HRESULT STDMETHODCALLTYPE SetClipStatusHook(IDirect3DDevice9* d,const D3DCLIPSTATUS9* value){
    if(!g_primary_device.Owns(d))return g_set_clip_status(d,value);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_set_clip_status(d,value);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,33u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::ClipStatus,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessingHook(IDirect3DDevice9* d,BOOL enable){
    if(!g_primary_device.Owns(d))return g_set_software_vp(d,enable);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_set_software_vp(d,enable);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,34u,rva);
    if(rva && SUCCEEDED(hr)){
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::SoftwareVertexProcessing,rva,hr);
        g_original_raster_mode_state.SuccessfulSoftwareVertexWrite(enable!=FALSE);
        const auto unsafe=g_original_raster_mode_state.UnsupportedMask();
        if(unsafe)Model2VR_RejectOriginalD3D9RasterMode(
            g_present_count.load(std::memory_order_acquire)+1u,unsafe,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE SetNPatchModeHook(IDirect3DDevice9* d,float segments){
    if(!g_primary_device.Owns(d))return g_set_npatch_mode(d,segments);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_set_npatch_mode(d,segments);
    const auto rva=CallerRva(origin);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,35u,rva);
    if(rva && SUCCEEDED(hr)){
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::NPatchMode,rva,hr);
        g_original_raster_mode_state.SuccessfulNPatchWrite(segments);
        const auto unsafe=g_original_raster_mode_state.UnsupportedMask();
        if(unsafe)Model2VR_RejectOriginalD3D9RasterMode(
            g_present_count.load(std::memory_order_acquire)+1u,unsafe,rva);
    }
    return hr;
}
// Original EXE COM runs exactly once. Unmirrored geometry cannot publish a
// speculative CPU/GPU eye pair; source patch/vertex bytes stay private.
void RejectUnmirrored(unsigned method,void* caller,std::uint64_t ordinal,HRESULT hr){
    const auto rva=CallerRva(caller);
    if(!rva)return;
    // Saturate instead of rolling over if a malformed original game loops.
    auto& calls=g_original_unmirrored_calls[method-1u];
    auto old=calls.load(std::memory_order_relaxed);
    while(old<10000000u && !calls.compare_exchange_weak(
        old,old+1u,std::memory_order_relaxed,std::memory_order_relaxed)){}
    const auto frame=g_present_count.load(std::memory_order_acquire)+1u;
    Model2VR_RejectOriginalD3D9UnmirroredGeometry(
        frame,method,rva,ordinal,static_cast<std::uint32_t>(hr));
    // Every affected physical frame needs an auditable fail-closed marker.
    // Limit to one per COM family in that frame, not every original mesh.
    if(method>=1u && method<=4u &&
       g_original_unmirrored_samples[method-1u].fetch_add(
           1u,std::memory_order_relaxed)==0u)
        m2vr::LogEvent("model2_d3d9_original_unmirrored_geometry",
            frame,rva,(std::uint64_t{method}<<32u)|ordinal,
            static_cast<std::uint32_t>(hr));
}
HRESULT STDMETHODCALLTYPE ProcessVerticesHook(IDirect3DDevice9* d,UINT src,UINT dst,
    UINT count,IDirect3DVertexBuffer9* vb,IDirect3DVertexDeclaration9* decl,DWORD flags){
    if(!g_primary_device.Owns(d))return g_process_vertices(d,src,dst,count,vb,decl,flags);
    void* origin=_ReturnAddress();
    const HRESULT hr=g_process_vertices(d,src,dst,count,vb,decl,flags);
    if(const auto rva=CallerRva(origin);rva && SUCCEEDED(hr)){
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::ProcessVertices,rva,hr);
        g_unmirrored_geometry_state.SuccessfulProcessVertices();
    }
    RejectUnmirrored(4u,origin,g_original_frame_draw_ordinal.load(std::memory_order_acquire),hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE DrawRectPatchHook(IDirect3DDevice9* d,UINT handle,
    const float* segments,const D3DRECTPATCH_INFO* info){
    if(!g_primary_device.Owns(d))return g_draw_rect_patch(d,handle,segments,info);
    const m2vr::drawprobe::ScopedOriginalDrawBoundary boundary;
    void* origin=_ReturnAddress();
    const auto ordinal=g_original_frame_draw_ordinal.fetch_add(1u,std::memory_order_acq_rel)+1u;
    const HRESULT hr=g_draw_rect_patch(d,handle,segments,info);
    RejectUnmirrored(1u,origin,ordinal,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE DrawTriPatchHook(IDirect3DDevice9* d,UINT handle,
    const float* segments,const D3DTRIPATCH_INFO* info){
    if(!g_primary_device.Owns(d))return g_draw_tri_patch(d,handle,segments,info);
    const m2vr::drawprobe::ScopedOriginalDrawBoundary boundary;
    void* origin=_ReturnAddress();
    const auto ordinal=g_original_frame_draw_ordinal.fetch_add(1u,std::memory_order_acq_rel)+1u;
    const HRESULT hr=g_draw_tri_patch(d,handle,segments,info);
    RejectUnmirrored(2u,origin,ordinal,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE DeletePatchHook(IDirect3DDevice9* d,UINT handle){
    if(!g_primary_device.Owns(d))return g_delete_patch(d,handle);
    void* origin=_ReturnAddress();
    const HRESULT hr=g_delete_patch(d,handle);
    RejectUnmirrored(3u,origin,g_original_frame_draw_ordinal.load(std::memory_order_acquire),hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetPSConstantFHook(IDirect3DDevice9* d,UINT start,
                                             const float* values,UINT count){
    if(!g_primary_device.Owns(d))return g_set_ps_constant_f(d,start,values,count);
    void* const caller=_ReturnAddress();
    // Internal experimental mirrored eyes are NOT original game source
    // program/matrix writes, and must not taint real hardware Present.
    if(m2vr::daytona::IsEyeReplayActive())return g_set_ps_constant_f(d,start,values,count);
    const auto source_write=g_original_camera_lifetime.EnterWrite();
    const HRESULT hr=g_set_ps_constant_f(d,start,values,count);
    g_original_camera_lifetime.CompleteWrite(
        source_write,CallerRva(caller)!=0u,SUCCEEDED(hr));
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,16u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::PixelConstants,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetTextureHook(IDirect3DDevice9* d,DWORD stage,IDirect3DBaseTexture9* texture){
    if(!g_primary_device.Owns(d))return g_set_texture(d,stage,texture);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_set_texture(d,stage,texture);
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,17u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::TextureBinding,rva,hr);
    if(rva && SUCCEEDED(hr) && texture)
        ObserveOriginalBoundTexture(texture,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetTextureStageStateHook(IDirect3DDevice9* d,DWORD stage,
                                                   D3DTEXTURESTAGESTATETYPE type,DWORD value){
    if(!g_primary_device.Owns(d))return g_set_texture_stage(d,stage,type,value);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_set_texture_stage(d,stage,type,value);
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,18u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::TextureStage,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetSamplerStateHook(IDirect3DDevice9* d,DWORD sampler,
                                              D3DSAMPLERSTATETYPE type,DWORD value){
    if(!g_primary_device.Owns(d))return g_set_sampler_state(d,sampler,type,value);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_set_sampler_state(d,sampler,type,value);
    const auto rva=CallerRva(caller);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,19u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::SamplerState,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetRenderStateHook(IDirect3DDevice9* d,D3DRENDERSTATETYPE state,DWORD value){
    if(!g_primary_device.Owns(d))return g_set_render_state(d,state,value);
    void* const caller=_ReturnAddress();
    // All write-profile mutations have entry-before-real-COM tickets:
    // otherwise a SetRenderState overlapping Draw/Present could promote
    // an obsolete cached direct-eye raster profile after a color/stencil
    // state change. Original COM invocation and HRESULT remain unmodified.
    const bool raster_state=state==D3DRS_ZENABLE ||
        state==D3DRS_ZWRITEENABLE || state==D3DRS_SCISSORTESTENABLE ||
        state==D3DRS_COLORWRITEENABLE || state==D3DRS_STENCILENABLE ||
        state==D3DRS_ZFUNC || state==D3DRS_CLIPPLANEENABLE;
    const auto token=raster_state?
        g_original_target_draw_coverage.EnterBind():
        m2vr::drawprobe::OriginalPhysicalTargetDrawCoverage::BindToken{};
    const HRESULT hr=g_set_render_state(d,state,value);
    const auto rva=CallerRva(caller);
    if(raster_state)
        g_original_target_draw_coverage.CompleteRasterMutation(
            token,rva!=0u,SUCCEEDED(hr));
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,20u,rva);
    RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::RenderState,rva,hr);
    return hr;
}
// Original COM state writes execute exactly once. On failure, revoke both
// verified CPU-world and speculative GPU-eye source for the physical Present.
HRESULT STDMETHODCALLTYPE SetViewportHook(IDirect3DDevice9* d,
                                           const D3DVIEWPORT9* vp){
    if(!g_primary_device.Owns(d))return g_set_viewport(d,vp);
    void* const caller=_ReturnAddress();
    const auto token=g_original_target_draw_coverage.EnterBind();
    const HRESULT hr=g_set_viewport(d,vp);
    const auto rva=CallerRva(caller);
    g_original_target_draw_coverage.CompleteRasterMutation(
        token,rva!=0u,SUCCEEDED(hr));
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,8u,rva);
    else if(rva && SUCCEEDED(hr) && vp)
        LogOriginalRegionPhase(d,1u,rva,vp->X,vp->Y,vp->Width,vp->Height);
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalLineageKind::Viewport,rva,hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE SetScissorRectHook(IDirect3DDevice9* d,
                                             const RECT* rect){
    if(!g_primary_device.Owns(d))return g_set_scissor(d,rect);
    void* const caller=_ReturnAddress();
    const auto token=g_original_target_draw_coverage.EnterBind();
    const HRESULT hr=g_set_scissor(d,rect);
    const auto rva=CallerRva(caller);
    g_original_target_draw_coverage.CompleteRasterMutation(
        token,rva!=0u,SUCCEEDED(hr));
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,9u,rva);
    else if(rva && SUCCEEDED(hr) && rect)
        LogOriginalRegionPhase(d,2u,rva,rect->left,rect->top,
            std::int64_t{rect->right}-rect->left,
            std::int64_t{rect->bottom}-rect->top);
    RecordOriginalLineageWrite(
        m2vr::drawprobe::OriginalLineageKind::Scissor,rva,hr);
    return hr;
}
// Dynamic original IDirect3DStateBlock9::Apply vtable hook. The device's
// CreateStateBlock and EndStateBlock paths register the runtime's actual
// COM implementation without reading original state-block contents.
HRESULT STDMETHODCALLTYPE StateBlockApplyHook(IDirect3DStateBlock9* block){
    void* const origin=_ReturnAddress();
    IDirect3DDevice9* device=nullptr;
    const HRESULT owner_result=block?block->GetDevice(&device):E_POINTER;
    const bool primary=SUCCEEDED(owner_result) && device &&
                       g_primary_device.Owns(device);
    if(device)device->Release();
    const HRESULT hr=g_state_block_apply(block); // original COM exactly once
    const auto rva=CallerRva(origin);
    if(!rva)return hr; // never attribute internal mirror/other DLL calls
    const auto frame=g_present_count.load(std::memory_order_acquire)+1u;
    if(primary){
        // An authentic state-block may restore RT0/depth/MRT without calling
        // the individually hooked Device9 SetRenderTarget/SetDepth methods.
        g_original_target_draw_coverage.InvalidateOpaqueStateBlock();
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::StateBlockApply,rva,hr);
        // Even successful Apply may replace unobserved state. Discard the
        // whole physical source frame; do NOT tamper with original mono.
        Model2VR_RejectOriginalD3D9StateBlockMutation(
            frame,1u,rva,static_cast<std::uint32_t>(hr));
    }else if(FAILED(owner_result)){
        g_state_block_apply_unobserved.store(true,std::memory_order_release);
        Model2VR_RejectOriginalD3D9StateBlockMutation(
            frame,2u,rva,static_cast<std::uint32_t>(hr));
    }
    return hr;
}
bool EnsureStateBlockApplyObserved(IDirect3DStateBlock9* block){
    if(!block)return false;
    void* const target=(*reinterpret_cast<void***>(block))[5];
    if(!target)return false;
    std::lock_guard lock(g_state_block_hook_mutex);
    if(g_state_block_apply_target)
        return target==g_state_block_apply_target && g_state_block_apply;
    if(!Hook(target,reinterpret_cast<void*>(&StateBlockApplyHook),
             g_state_block_apply))return false;
    g_state_block_apply_target=target;
    return true;
}
void StateBlockHookCoverageFailed(std::uint32_t rva)noexcept{
    g_state_block_apply_unobserved.store(true,std::memory_order_release);
    g_original_target_draw_coverage.InvalidateOpaqueStateBlock();
    if(rva)Model2VR_RejectOriginalD3D9StateBlockMutation(
        g_present_count.load(std::memory_order_acquire)+1u,2u,rva,0u);
}
HRESULT STDMETHODCALLTYPE CreateStateBlockHook(IDirect3DDevice9* d,
    D3DSTATEBLOCKTYPE type,IDirect3DStateBlock9** out){
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_create_state_block(d,type,out);
    if(g_primary_device.Owns(d) && SUCCEEDED(hr) && out && *out &&
       !EnsureStateBlockApplyObserved(*out))
        StateBlockHookCoverageFailed(CallerRva(origin));
    return hr;
}
HRESULT STDMETHODCALLTYPE BeginStateBlockHook(IDirect3DDevice9* d){
    // Recording itself is not a safe indication of state Apply. Never
    // fabricate a source mutation while the original block is constructed.
    return g_begin_state_block(d);
}
HRESULT STDMETHODCALLTYPE EndStateBlockHook(IDirect3DDevice9* d,
    IDirect3DStateBlock9** out){
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_end_state_block(d,out);
    if(g_primary_device.Owns(d) && SUCCEEDED(hr) && out && *out &&
       !EnsureStateBlockApplyObserved(*out))
        StateBlockHookCoverageFailed(CallerRva(origin));
    return hr;
}
// Every COM Present path must execute the original game's method exactly
// once; a secondary device is excluded from all primary frame accounting.
HRESULT CallOriginalPresentOnce(IDirect3DDevice9* d,CONST RECT* a,
    CONST RECT* b,HWND w,CONST RGNDATA* dirty){
    return g_present(d,a,b,w,dirty);
}
HRESULT STDMETHODCALLTYPE PresentHook(IDirect3DDevice9* d,CONST RECT* a,CONST RECT* b,
                                     HWND w,CONST RGNDATA* dirty){
    if(!g_primary_device.Owns(d)){
        const unsigned seen=g_secondary_presents.fetch_add(
            1u,std::memory_order_relaxed)+1u;
        if(seen==1u || seen%120u==0u)
            m2vr::LogEvent("model2_d3d9_secondary_device_present",
                           seen,0u,0u,0u);
        return CallOriginalPresentOnce(d,a,b,w,dirty);
    }
    const bool startup_first_present=
        g_present_count.load(std::memory_order_acquire)==0u;
    TraceFirstOriginalPresentStage(startup_first_present,1u,1u);
    // Original dialogue overlay and device gamma can change actual displayed
    // content without another Draw; never reuse unverified eye surfaces.
    // Hardware/GDI/emulated cursor output is not authenticated XR eye content.
    if(g_original_cursor_visible.load(std::memory_order_acquire) ||
       g_original_cursor_unknown.load(std::memory_order_acquire))
        BeforeOriginalPresentationControl(0u,9u);
    if(g_original_query_source_unproven.load(std::memory_order_acquire))
        OriginalQuerySourceBoundary(0u,3u);
    // A physical Present cannot use an unproven native ValidateDevice pass
    // count even if a later Draw supplied an otherwise matching camera nonce.
    if(g_original_validation_unproven.load(std::memory_order_acquire))
        OriginalValidationSourceBoundary(0u,5u);
    if(g_residency_foreign_unproven.load(std::memory_order_acquire))
        OriginalResidencySourceBoundary(0u,3u);
    if(g_original_dialog_box_active.load(std::memory_order_acquire))
        BeforeOriginalPresentationControl(0u,3u);
    if(g_original_gamma_unproven.load(std::memory_order_acquire))
        BeforeOriginalPresentationControl(0u,4u);
    if(g_original_external_presentation_unproven.load(std::memory_order_acquire))
        BeforeOriginalPresentationControl(0u,5u);
    // A genuine additional SwapChain::Present is a distinct physical
    // presentation, not a device Present nonce. Never republish its old eyes.
    if(g_original_swap_chain_source_unproven.load(std::memory_order_acquire))
        Model2VR_RejectOriginalD3D9SwapChainPresent(
            g_present_count.load(std::memory_order_acquire)+1u,0u,2u);
    // If a writable original resource is still mapped at Present, the
    // game may modify it without another Lock. Future stereo must remain
    // disarmed, not silently reuse a previous eye geometry source.
    if(g_original_bound_write_locks.HasOutstandingWrites())
        OriginalBufferCoverageUnknown(0u,0u);
    if(g_original_texture_write_locks.HasOutstandingWrites())
        OriginalTextureCoverageUnknown(0u);
    if(g_texture_hook_unknown.load(std::memory_order_acquire))
        Model2VR_RejectOriginalD3D9BufferHookCoverage(
            g_present_count.load(std::memory_order_acquire)+1u,0u,0u);
    if(g_buffer_hook_unknown.load(std::memory_order_acquire))
        Model2VR_RejectOriginalD3D9BufferHookCoverage(
            g_present_count.load(std::memory_order_acquire)+1u,0u,0u);
    // Check persisted source stream dividers BEFORE advancing Present ID:
    // reject speculative eyes even with no SetStreamSourceFreq this frame.
    if(g_unmirrored_geometry_state.HasProcessedVertices())
        Model2VR_RejectOriginalD3D9UnmirroredGeometry(
            g_present_count.load(std::memory_order_acquire)+1u,5u,0u,0u,0u);
    const auto raster_mask=g_original_raster_mode_state.UnsupportedMask();
    if(raster_mask)Model2VR_RejectOriginalD3D9RasterMode(
        g_present_count.load(std::memory_order_acquire)+1u,raster_mask,0u);
    const auto active_freq=g_original_stream_frequency_state.UnsupportedMask();
    if(active_freq)
        Model2VR_RejectOriginalD3D9ActiveInstancing(
            g_present_count.load(std::memory_order_acquire)+1u,active_freq);
    // NextOriginalD3D9PresentFrameId() is defined as the current Present
    // counter + 1. Revoke incomplete source stereo BEFORE incrementing that
    // counter, or the bridge would silently reject a wrong-frame request.
    const unsigned scene_source_frame=
        g_present_count.load(std::memory_order_acquire)+1u;
    // Cut the physical ORIGINAL Scene/Draw epoch BEFORE resetting its source
    // scene/Draw counters. The genuine Present executes exactly once below.
    const auto original_scene_coverage=g_original_scene_draw_coverage.FinishPresent(
        g_original_scene_open.load(std::memory_order_acquire),
        g_original_scene_poisoned.load(std::memory_order_acquire),
        g_original_scene_hooks_ready.load(std::memory_order_acquire));
    // RT0/depth evidence is bound to this very SAME physical original
    // Scene/Draw epoch, not a sampled frame or an inferred world owner.
    auto original_target_coverage=g_original_target_draw_coverage.FinishPresent();
    const auto camera_com_coverage=g_original_camera_lifetime.FinishPresent();
    // Lifetime counts include unsampled genuine original Draw families.
    const auto camera_source_flags=camera_com_coverage.flags |
        (camera_com_coverage.draws!=original_scene_coverage.TotalDraws()
            ?m2vr::drawprobe::OriginalCameraComLifetime::kRevisionDrift:0u);
    if(camera_source_flags)
        Model2VR_RejectOriginalD3D9CameraComLifetime(
            scene_source_frame,camera_source_flags,
            camera_com_coverage.draws,camera_com_coverage.writes);
    if(original_target_coverage.TotalDraws()!=original_scene_coverage.TotalDraws())
        original_target_coverage.flags|=
            m2vr::drawprobe::OriginalPhysicalTargetDrawCoverage::kCountMismatch;
    if(original_target_coverage.RejectUnprovenStereoSource())
        Model2VR_RejectOriginalD3D9TargetDrawCoverage(
            scene_source_frame,original_target_coverage.flags,
            original_target_coverage.unknown,
            original_target_coverage.incompatible_depth);
    // Real game source frame is no longer fully Scene-owned. Revoke BOTH
    // partially prepared CPU and GPU eyes before physical COM Present.
    // Never change the source D3D9 call, displayed mono or game input/FFB.
    if(original_scene_coverage.RejectUnprovenStereoSource())
        Model2VR_RejectOriginalD3D9SceneDrawCoverage(
            scene_source_frame,original_scene_coverage.flags,
            original_scene_coverage.successful_untrusted,
            original_scene_coverage.failed_draws);
    const unsigned frame=g_present_count.fetch_add(1,std::memory_order_relaxed)+1;
    // An unobserved state-block Apply vtable stays unsafe across Reset and
    // Present. Keep the HMD on original mono, not stale speculative eyes.
    if(g_state_block_apply_unobserved.load(std::memory_order_acquire) &&
       g_native_stereo)m2vr::daytona::MarkOriginalLineageMismatch();
    g_original_frame_draw_ordinal.exchange(0u,std::memory_order_relaxed);
    g_original_scene_ordinal.exchange(0u,std::memory_order_relaxed);
    g_original_scene_poisoned.store(false,std::memory_order_release);
    g_original_scene_open.store(false,std::memory_order_release);
    g_original_scene_owner_thread.store(0u,std::memory_order_release);
    for(auto& revision:g_original_lineage_writes)
        revision.store(0u,std::memory_order_release);
    // Native replay must also drain the *original* source-family counters.
    // Previously these only reset in passive 2D mode, so a long VR session
    // accumulated lifetime totals instead of per-Present evidence.
    const unsigned dp=g_draws.exchange(0u,std::memory_order_relaxed);
    const unsigned dip=g_indexed.exchange(0u,std::memory_order_relaxed);
    const unsigned dpup=g_up.exchange(0u,std::memory_order_relaxed);
    const unsigned dipup=g_indexed_up.exchange(0u,std::memory_order_relaxed);
    std::array<std::uint32_t,4> unmirrored_counts{};
    for(std::size_t i=0;i<unmirrored_counts.size();++i)
        unmirrored_counts[i]=g_original_unmirrored_calls[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,2> buffer_creates{};
    for(std::size_t i=0;i<buffer_creates.size();++i)
        buffer_creates[i]=g_original_buffer_creates[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,3> original_creates{};
    for(std::size_t i=0;i<original_creates.size();++i)
        original_creates[i]=g_original_texture_creates[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,3> surface_creates{};
    for(std::size_t i=0;i<surface_creates.size();++i)
        surface_creates[i]=g_original_surface_creates[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,4> original_query_calls{};
    for(std::size_t i=0;i<original_query_calls.size();++i)
        original_query_calls[i]=g_original_query_calls[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,3> original_cursor_controls{};
    for(std::size_t i=0;i<original_cursor_controls.size();++i)
        original_cursor_controls[i]=g_original_cursor_controls[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,2> original_presentation_controls{};
    for(std::size_t i=0;i<original_presentation_controls.size();++i)
        original_presentation_controls[i]=g_original_presentation_controls[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,4> original_swap_chain_calls{};
    for(std::size_t i=0;i<original_swap_chain_calls.size();++i)
        original_swap_chain_calls[i]=g_original_swap_chain_calls[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,3> original_resource_getters{};
    for(std::size_t i=0;i<original_resource_getters.size();++i)
        original_resource_getters[i]=g_original_resource_getters[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,4> gpu_surface_writes{};
    for(std::size_t i=0;i<gpu_surface_writes.size();++i)
        gpu_surface_writes[i]=g_original_gpu_surface_writes[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,3> surface_getters{};
    for(std::size_t i=0;i<surface_getters.size();++i)
        surface_getters[i]=g_original_surface_getters[i].exchange(
            0u,std::memory_order_relaxed);
    std::array<std::uint32_t,2> original_readbacks{};
    for(std::size_t i=0;i<original_readbacks.size();++i)
        original_readbacks[i]=g_original_surface_readbacks[i].exchange(
            0u,std::memory_order_relaxed);
    const auto surface_locks=g_original_texture_surface_writes.exchange(
        0u,std::memory_order_relaxed);
    const auto cube_locks=g_original_texture_shape_writes[0].exchange(
        0u,std::memory_order_relaxed);
    const auto volume_locks=g_original_texture_shape_writes[1].exchange(
        0u,std::memory_order_relaxed);
    const auto texture_locks=g_original_texture_lock_writes.exchange(
        0u,std::memory_order_relaxed);
    const auto vb_write=g_original_buffer_writes[0].exchange(
        0u,std::memory_order_relaxed);
    const auto ib_write=g_original_buffer_writes[1].exchange(
        0u,std::memory_order_relaxed);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_buffer_creation_counts",
            frame,buffer_creates[0],buffer_creates[1],0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_texture_creation_counts",
            frame,original_creates[0],original_creates[1],original_creates[2]);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_surface_creation_counts",
            frame,surface_creates[0],surface_creates[1],surface_creates[2]);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_surface_readback_counts",
            frame,original_readbacks[0],original_readbacks[1],0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_surface_getter_counts",
            frame,surface_getters[0],surface_getters[1],surface_getters[2]);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_gpu_surface_write_counts",
            frame,
            (std::uint64_t{gpu_surface_writes[0]}<<32u)|gpu_surface_writes[1],
            (std::uint64_t{gpu_surface_writes[2]}<<32u)|gpu_surface_writes[3],
            0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_presentation_control_counts",
            frame,original_presentation_controls[0],original_presentation_controls[1],
            (g_original_dialog_box_active.load(std::memory_order_acquire)?1u:0u)|
            (g_original_gamma_unproven.load(std::memory_order_acquire)?2u:0u)|
            (g_original_external_presentation_unproven.load(std::memory_order_acquire)?4u:0u));
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_query_counts",frame,
            (std::uint64_t{original_query_calls[0]}<<32u)|original_query_calls[1],
            (std::uint64_t{original_query_calls[2]}<<32u)|original_query_calls[3],
            g_original_query_source_unproven.load(std::memory_order_acquire)?1u:0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_cursor_control_counts",frame,
            (std::uint64_t{original_cursor_controls[0]}<<32u)|original_cursor_controls[1],
            original_cursor_controls[2],
            (g_original_cursor_visibility_known.load(std::memory_order_acquire)?1u:0u)|
            (g_original_cursor_visible.load(std::memory_order_acquire)?2u:0u)|
            (g_original_cursor_unknown.load(std::memory_order_acquire)?4u:0u));
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_swap_chain_counts",frame,
            (std::uint64_t{original_swap_chain_calls[0]}<<32u)|original_swap_chain_calls[1],
            (std::uint64_t{original_swap_chain_calls[2]}<<32u)|original_swap_chain_calls[3],
            g_original_swap_chain_source_unproven.load(std::memory_order_acquire)?1u:0u);
    const auto swap_readback=g_original_swap_chain_readbacks.exchange(
        0u,std::memory_order_relaxed);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_swap_chain_readback_counts",
            frame,swap_readback,
            g_original_swap_chain_source_unproven.load(std::memory_order_acquire)?1u:0u,0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_resource_getter_counts",
            frame,original_resource_getters[0],
            original_resource_getters[1],original_resource_getters[2]);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_texture_lock_counts",
            frame,texture_locks,
            g_texture_hook_unknown.load(std::memory_order_acquire)?1u:0u,0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_texture_shape_lock_counts",
            frame,cube_locks,volume_locks,
            g_texture_hook_unknown.load(std::memory_order_acquire)?1u:0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_texture_surface_lock_counts",
            frame,surface_locks,
            g_texture_hook_unknown.load(std::memory_order_acquire)?1u:0u,0u);
    if(frame==1u || frame%120u==0u)
        m2vr::LogEvent("model2_d3d9_original_buffer_lock_counts",
            frame,vb_write,ib_write,
            g_buffer_hook_unknown.load(std::memory_order_acquire)?1u:0u);
    if(frame==1u || frame%120u==0u){
        // Physical Present aggregate: rect patch, tri patch, delete patch,
        // ProcessVertices. This is diagnostic only, never World3D evidence.
        m2vr::LogEvent("model2_d3d9_original_unmirrored_com_counts",
            frame,
            (std::uint64_t{unmirrored_counts[0]}<<32u)|unmirrored_counts[1],
            (std::uint64_t{unmirrored_counts[2]}<<32u)|unmirrored_counts[3],
            g_unmirrored_geometry_state.HasProcessedVertices()?1u:0u);
    }
    // Query the authenticated source before the physical Present; this
    // permits staging original mono as backup instead of forcing two
    // speculative GPU eye readbacks that cannot win the frame arbitration.
    const bool verified_cpu_pending_pre=
        Model2VR_HasVerifiedWorldPendingForOriginalPresent(frame)!=FALSE;
    if(g_native_stereo){
        m2vr::daytona::PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre);
        // Aggregate original draw counts remain available even if the
        // HMD is disconnected, and never log game vertices or shaders.
        if(frame==1u || frame%120u==0u)
            m2vr::LogEvent("model2_vr_original_draw_families",frame,
                (std::uint64_t{dp}<<32)|dip,
                (std::uint64_t{dpup}<<32)|dipup);
    }else{
        m2vr::LogEvent("model2_d3d9_frame",frame,dp+dip,dpup+dipup);
    }
    // Snapshot sampling state belongs to a frame, not a process lifetime.
    for(auto& counter:g_family_samples)
        counter.store(0u,std::memory_order_relaxed);
    for(auto& counter:g_original_mutation_samples)
        counter.store(0u,std::memory_order_relaxed);
    for(auto& counter:g_original_target_bind_samples)
        counter.store(0u,std::memory_order_relaxed);
    for(auto& counter:g_original_transform_samples)
        counter.store(0u,std::memory_order_relaxed);
    g_original_vs_constant_samples.store(0u,std::memory_order_relaxed);
    for(auto& sample:g_original_region_samples)sample.store(0u,std::memory_order_relaxed);
    for(auto& sample:g_original_unmirrored_samples)sample.store(0u,std::memory_order_relaxed);
    // Original Present runs exactly once. Preserve its HRESULT and publish
    // the staged GameStereo pair only if the real source Present succeeded.
    TraceFirstOriginalPresentStage(startup_first_present,2u,frame);
    const HRESULT original_result=g_present(d,a,b,w,dirty);
    // Original GPU Present is still a forbidden source-write window.
    // Reopen only AFTER the actual COM returns; otherwise a second render
    // thread could enter a new Scene/Draw while the prior Present executes.
    g_original_scene_draw_coverage.ReopenAfterPhysicalPresentReturn();
    g_original_target_draw_coverage.ReopenAfterPhysicalPresentReturn();
    g_original_camera_lifetime.ReopenAfterPhysicalPresentReturn();
    TraceFirstOriginalPresentStage(startup_first_present,3u,frame,
        static_cast<std::uint32_t>(original_result));
    const auto source_samples=g_original_source_result_samples.exchange(
        0u,std::memory_order_relaxed);
    if(source_samples)
        m2vr::LogEvent("model2_d3d9_original_present_result",frame,
            static_cast<std::uint32_t>(original_result),source_samples,0u);
    // Every eighth physical original Present (and the first) supplies
    // NEGATIVE-control coverage even when there are no sampled VB/IB
    // witnesses. Title/menu/sky/HUD/aux may use only UP family Draws.
    // This is genuine primary-device post-COM frame evidence, NOT a
    // World3D signal: four family counts and HRESULT only, no game pixels,
    // resource addresses, vertex data, GPU contents or emulation state.
    if(m2vr::drawprobe::OriginalControlPresentSample(
           frame,dp,dip,dpup,dipup))
        m2vr::LogEvent("model2_d3d9_original_control_present_phase",
            frame,static_cast<std::uint32_t>(original_result),
            m2vr::drawprobe::OriginalControlPresentPair(dp,dip),
            m2vr::drawprobe::OriginalControlPresentPair(dpup,dipup));
    // A cumulative fingerprint is emitted at EVERY real primary Present,
    // never only a sampled VB/IB or every-eighth UP frame. A PRIVATE capture
    // can therefore reject callers reused by menu/sky/HUD even when a Draw
    // was outside the 1-in-1024 detailed material witness cadence.
    // Bitwise OR is monotonic over the process lifetime; unknown EXE origin
    // forces conservative rejection by the offline negative-control gate.
    m2vr::LogEvent("model2_d3d9_original_all_family_caller_bloom",
        frame,g_original_draw_caller_bloom_lo.load(std::memory_order_acquire),
        g_original_draw_caller_bloom_hi.load(std::memory_order_acquire),
        g_original_draw_caller_unknown.load(std::memory_order_acquire)?1u:0u);
    // Exact bounded first-seen caller IDs disambiguate saturated bloom bits.
    // Keep the original game Draw/Present/VR eyes unchanged. The private
    // offline auditor must prove every event is accounted for each Present.
    m2vr::LogEvent("model2_d3d9_original_caller_inventory",
        frame,g_original_caller_inventory.Count(),
        g_original_caller_inventory.Incomplete()?1u:0u,
        g_original_draw_caller_unknown.load(std::memory_order_acquire)?1u:0u);
    // Unsampled, actual post-COM Present process boundary: all captured
    // frames from one original run share the same nonzero random nonce.
    // A fresh game process must have a distinct nonce for PRIVATE A/B controls.
    m2vr::LogEvent("model2_d3d9_original_capture_run",
        frame,OriginalPrivateCaptureRunNonce(),0u,0u);
    // All four genuine COM Draw families, including UNSAMPLED VB and UP,
    // are counted by actual original result + source scene ownership.
    m2vr::LogEvent("model2_d3d9_original_scene_draw_coverage",
        frame,
        (std::uint64_t{original_scene_coverage.begins}<<32u)|
            original_scene_coverage.ends,
        (std::uint64_t{original_scene_coverage.successful_in_scene}<<32u)|
            original_scene_coverage.successful_untrusted,
        (std::uint64_t{original_scene_coverage.failed_draws}<<32u)|
            original_scene_coverage.flags);
    // Continuous source RT identity class and depth validity per Present.
    m2vr::LogEvent("model2_d3d9_original_camera_com_lifetime",
        frame,camera_source_flags,camera_com_coverage.draws,
        camera_com_coverage.writes);
    m2vr::LogEvent("model2_d3d9_original_draw_target_coverage",
        frame,
        (std::uint64_t{original_target_coverage.backbuffer}<<32u)|
            original_target_coverage.auxiliary,
        (std::uint64_t{original_target_coverage.unknown}<<32u)|
            original_target_coverage.failed,
        (std::uint64_t{original_target_coverage.incompatible_depth}<<32u)|
            original_target_coverage.flags);
    m2vr::drawprobe::NextProjectDrawEpoch();
    // Both source paths staged their images BEFORE the single physical
    // game Present. An explicitly authenticated same-frame CPU-world pair
    // wins only after its original HRESULT succeeds and pose stays live.
    const bool verified_cpu_pending=verified_cpu_pending_pre &&
        Model2VR_HasVerifiedWorldPendingForOriginalPresent(frame)!=FALSE;
    const auto stereo_mode=m2vr::vrbridge::SelectOriginalStereoSourceMode(
        g_native_stereo,verified_cpu_pending);
    const bool verified_cpu_published=
        Model2VR_CompleteVerifiedWorldOriginalPresent(
            frame,SUCCEEDED(original_result)?1u:0u,
            static_cast<unsigned int>(stereo_mode))!=FALSE;
    if(g_native_stereo){
        if(verified_cpu_published){
            // Do not publish a second GameStereo packet for this frame or
            // overwrite producer health from the verified CPU world.
            m2vr::daytona::DiscardPreparedFrameAfterCpuWorld(frame);
        }else{
            // Absent/expired/wrong-owner world: preserve experimental eye
            // replay and original 2D backbuffer fallback, no extra game Draw.
            m2vr::daytona::FinishFrameAfterPresent(frame,original_result);
        }
    }
    return original_result;
}
HRESULT STDMETHODCALLTYPE ResetHook(IDirect3DDevice9* d,
                                     D3DPRESENT_PARAMETERS* pp){
    if(!g_primary_device.Owns(d))return g_reset(d,pp);
    g_original_frame_draw_ordinal.store(0u,std::memory_order_relaxed);
    g_original_scene_ordinal.store(0u,std::memory_order_relaxed);
    for(auto& counter:g_original_mutation_samples)
        counter.store(0u,std::memory_order_relaxed);
    for(auto& counter:g_original_target_bind_samples)
        counter.store(0u,std::memory_order_relaxed);
    for(auto& counter:g_original_transform_samples)
        counter.store(0u,std::memory_order_relaxed);
    g_original_vs_constant_samples.store(0u,std::memory_order_relaxed);
    for(auto& sample:g_original_region_samples)sample.store(0u,std::memory_order_relaxed);
    for(auto& sample:g_original_unmirrored_samples)sample.store(0u,std::memory_order_relaxed);
    for(auto& calls:g_original_unmirrored_calls)calls.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_buffer_writes)count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_buffer_creates)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_texture_creates)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_surface_creates)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_surface_readbacks)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_surface_getters)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_gpu_surface_writes)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_resource_getters)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_swap_chain_calls)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_presentation_controls)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_cursor_controls)
        count.store(0u,std::memory_order_relaxed);
    for(auto& count:g_original_query_calls)
        count.store(0u,std::memory_order_relaxed);
    // Unknown original Query coverage persists across genuine Reset.
    // Reset alone does not establish a calibrated HMD gamma or clear a
    // confirmed original dialog mode; retain both fail-closed guards.
    g_original_swap_chain_readbacks.store(0u,std::memory_order_relaxed);
    // SwapChain source uncertainty persists across Reset: no remapping to
    // the primary physical Present is proven by a device Reset alone.
    g_original_texture_lock_writes.store(0u,std::memory_order_relaxed);
    g_original_texture_surface_writes.store(0u,std::memory_order_relaxed);
    for(auto& n:g_original_texture_shape_writes)
        n.store(0u,std::memory_order_relaxed);
    for(auto& revision:g_original_lineage_writes)
        revision.store(0u,std::memory_order_release);
    g_original_scene_poisoned.store(false,std::memory_order_release);
    g_original_scene_open.store(false,std::memory_order_release);
    g_original_scene_owner_thread.store(0u,std::memory_order_release);
    g_original_scene_draw_coverage.Reset();
    g_original_target_draw_coverage.Reset();
    g_original_camera_lifetime.Reset();
    m2vr::drawprobe::NextProjectDrawEpoch();
    Model2VR_DiscardPendingWorldOnReset();
    if(g_native_stereo)m2vr::daytona::ResetResources();
    const HRESULT result=g_reset(d,pp);
    // Failed device Reset must not forget an active original instanced
    // stream setting; only a SUCCESSFUL real Reset returns to defaults.
    if(SUCCEEDED(result)){
        g_original_stream_frequency_state.AfterSuccessfulDeviceReset();
        g_original_raster_mode_state.AfterSuccessfulDeviceReset();
        g_unmirrored_geometry_state.AfterSuccessfulDeviceReset();
        g_original_bound_write_locks.AfterSuccessfulDeviceReset();
        g_original_texture_write_locks.AfterSuccessfulDeviceReset();
        // D3D9 cursor image is lost on successful Reset; old visibility
        // must not be assumed equivalent to the XR host cursor state.
        const bool unsafe=g_original_cursor_visible.load(std::memory_order_acquire) ||
            g_original_cursor_unknown.load(std::memory_order_acquire);
        g_original_cursor_visibility_known.store(false,std::memory_order_release);
        g_original_cursor_visible.store(false,std::memory_order_release);
        g_original_cursor_unknown.store(unsafe,std::memory_order_release);
    }
    return result;
}
bool HookDevice(IDirect3DDevice9* dev){
    if(!dev)return false;
    std::lock_guard lock(g_setup_mutex);
    if(g_device_installed)return true;
    // COM IDirect3DDevice9 method slots per d3d9.h (IUnknown included).
    auto** vtable=*reinterpret_cast<void***>(dev);
    const bool p=Hook(vtable[17],reinterpret_cast<void*>(&PresentHook),g_present);
    const bool cursor_image=Hook(vtable[10],
        reinterpret_cast<void*>(&OriginalSetCursorPropertiesHook),g_original_set_cursor_properties);
    const bool cursor_position=Hook(vtable[11],
        reinterpret_cast<void*>(&OriginalSetCursorPositionHook),g_original_set_cursor_position);
    const bool cursor_visibility=Hook(vtable[12],
        reinterpret_cast<void*>(&OriginalShowCursorHook),g_original_show_cursor);
    const bool dialog=Hook(vtable[20],
        reinterpret_cast<void*>(&OriginalSetDialogBoxModeHook),g_original_set_dialog_box_mode);
    const bool gamma=Hook(vtable[21],
        reinterpret_cast<void*>(&OriginalSetGammaRampHook),g_original_set_gamma_ramp);
    const bool create_swap_chain=Hook(vtable[13],
        reinterpret_cast<void*>(&OriginalCreateAdditionalSwapChainHook),
        g_create_additional_swap_chain);
    const bool get_swap_chain=Hook(vtable[14],
        reinterpret_cast<void*>(&OriginalGetSwapChainHook),g_get_swap_chain);
    // Safe startup after original hardware Query crashes: opt-in only.
    const bool validate=Hook(vtable[70],
        reinterpret_cast<void*>(&OriginalValidateDeviceHook),g_validate_device);
    const bool evict=Hook(vtable[5],
        reinterpret_cast<void*>(&OriginalEvictManagedResourcesHook),
        g_evict_managed_resources);
    const bool create_query=!g_query_hooks_enabled || Hook(vtable[118],
        reinterpret_cast<void*>(&OriginalCreateQueryHook),g_create_query);
    const bool dp=Hook(vtable[81],reinterpret_cast<void*>(&DrawHook),g_draw);
    const bool di=Hook(vtable[82],reinterpret_cast<void*>(&IndexedHook),g_draw_indexed);
    const bool du=Hook(vtable[83],reinterpret_cast<void*>(&DrawUPHook),g_draw_up);
    const bool iu=Hook(vtable[84],reinterpret_cast<void*>(&IndexedUPHook),g_draw_indexed_up);
    // Even passive mode can host an explicitly verified CPU-world frame.
    // Always discard its pending eye pair on original device Reset.
    const bool reset=Hook(
        vtable[16],reinterpret_cast<void*>(&ResetHook),g_reset);
    // Always observe real source Clear HRESULTs, including CPU-only mode.
    const bool clear=Hook(vtable[43],reinterpret_cast<void*>(&ClearHook),g_clear);
    // Failed source transfers invalidate CPU world even in non-GPU mode.
    // They are pure original COM observers unless native-eye replay is on.
    // Authenticate original texture creation, before SetTexture is called.
    // This covers CPU writes / generated mips on a newly-created unbound
    // texture, not only source mutations after an observed material bind.
    const bool make_tex=Hook(vtable[23],reinterpret_cast<void*>(&OriginalCreateTextureHook),
                             g_create_texture);
    const bool make_volume=Hook(vtable[24],reinterpret_cast<void*>(&OriginalCreateVolumeTextureHook),
                                g_create_volume_texture);
    const bool make_cube=Hook(vtable[25],reinterpret_cast<void*>(&OriginalCreateCubeTextureHook),
                              g_create_cube_texture);
    const bool make_vb=Hook(vtable[26],reinterpret_cast<void*>(&OriginalCreateVertexBufferHook),
                            g_create_vertex_buffer);
    const bool make_ib=Hook(vtable[27],reinterpret_cast<void*>(&OriginalCreateIndexBufferHook),
                            g_create_index_buffer);
    // Capture standalone Surface sources before their first LockRect/GetDC.
    const bool make_rt=Hook(vtable[28],
        reinterpret_cast<void*>(&OriginalCreateRenderTargetHook),g_create_render_target);
    const bool make_depth_surface=Hook(vtable[29],
        reinterpret_cast<void*>(&OriginalCreateDepthStencilSurfaceHook),
        g_create_depth_stencil_surface);
    const bool make_offscreen=Hook(vtable[36],
        reinterpret_cast<void*>(&OriginalCreateOffscreenPlainSurfaceHook),
        g_create_offscreen_plain_surface);
    // Original read-only Surface accessor aliases can be subsequently mapped
    // without Create* or SetRenderTarget. Observe before first Lock/GetDC.
    const bool get_back=Hook(vtable[18],
        reinterpret_cast<void*>(&OriginalGetBackBufferHook),g_get_back_buffer);
    const bool get_rt=Hook(vtable[38],
        reinterpret_cast<void*>(&OriginalGetRenderTargetHook),g_get_render_target);
    const bool get_depth=Hook(vtable[40],
        reinterpret_cast<void*>(&OriginalGetDepthStencilSurfaceHook),g_get_depth_stencil_surface);
    // Actual original GPU->CPU Surface writes can occur before the first bind.
    const bool read_rt=Hook(vtable[32],
        reinterpret_cast<void*>(&OriginalGetRenderTargetDataHook),g_get_render_target_data);
    const bool read_front=Hook(vtable[33],
        reinterpret_cast<void*>(&OriginalGetFrontBufferDataHook),g_get_front_buffer_data);
    const bool update=Hook(vtable[30],reinterpret_cast<void*>(&UpdateSurfaceHook),g_update_surface);
    const bool texture_upload=Hook(vtable[31],reinterpret_cast<void*>(&UpdateTextureHook),g_update_texture);
    const bool palette_entries=Hook(vtable[71],reinterpret_cast<void*>(&SetPaletteEntriesHook),g_set_palette_entries);
    const bool current_palette=Hook(vtable[73],reinterpret_cast<void*>(&SetCurrentTexturePaletteHook),g_set_current_texture_palette);
    const bool stretch=Hook(vtable[34],reinterpret_cast<void*>(&StretchRectHook),g_stretch_rect);
    const bool fill=Hook(vtable[35],reinterpret_cast<void*>(&ColorFillHook),g_color_fill);
    // IDirect3DDevice9 SetRenderTarget=37, SetDepthStencilSurface=39.
    const bool rt=Hook(vtable[37],reinterpret_cast<void*>(&SetRenderTargetHook),
                       g_set_render_target);
    const bool depth=Hook(vtable[39],
        reinterpret_cast<void*>(&SetDepthStencilSurfaceHook),g_set_depth_surface);
    const bool transform=Hook(vtable[44],
        reinterpret_cast<void*>(&SetTransformHook),g_set_transform);
    const bool multiply_transform=Hook(vtable[46],
        reinterpret_cast<void*>(&MultiplyTransformHook),g_multiply_transform);
    const bool vs_constants=Hook(vtable[94],
        reinterpret_cast<void*>(&SetVSConstantFHook),g_set_vs_constant_f);
    const bool viewport=Hook(vtable[47],reinterpret_cast<void*>(&SetViewportHook),g_set_viewport);
    const bool scissor=Hook(vtable[75],reinterpret_cast<void*>(&SetScissorRectHook),g_set_scissor);
    // Six original D3D9 bindings complete the one-shot Draw source nonce.
    const bool stream_source=Hook(vtable[100],reinterpret_cast<void*>(&SetStreamSourceHook),g_set_stream_source);
    const bool get_stream=Hook(vtable[101],
        reinterpret_cast<void*>(&OriginalGetStreamSourceHook),g_get_stream_source);
    const bool indices=Hook(vtable[104],reinterpret_cast<void*>(&SetIndicesHook),g_set_indices);
    const bool get_indices=Hook(vtable[105],
        reinterpret_cast<void*>(&OriginalGetIndicesHook),g_get_indices);
    const bool vertex_declaration=Hook(vtable[87],reinterpret_cast<void*>(&SetVertexDeclarationHook),g_set_vertex_declaration);
    const bool f_v_f=Hook(vtable[89],reinterpret_cast<void*>(&SetFVFHook),g_set_f_v_f);
    const bool vertex_shader=Hook(vtable[92],reinterpret_cast<void*>(&SetVertexShaderHook),g_set_vertex_shader);
    const bool pixel_shader=Hook(vtable[107],reinterpret_cast<void*>(&SetPixelShaderHook),g_set_pixel_shader);
    const bool ps_constants=Hook(vtable[109],reinterpret_cast<void*>(&SetPSConstantFHook),g_set_ps_constant_f);
    const bool texture=Hook(vtable[65],reinterpret_cast<void*>(&SetTextureHook),g_set_texture);
    const bool get_texture=Hook(vtable[66],
        reinterpret_cast<void*>(&OriginalGetTextureHook),g_get_texture);
    const bool texture_stage=Hook(vtable[67],reinterpret_cast<void*>(&SetTextureStageStateHook),g_set_texture_stage);
    const bool sampler=Hook(vtable[69],reinterpret_cast<void*>(&SetSamplerStateHook),g_set_sampler_state);
    const bool render_state=Hook(vtable[57],reinterpret_cast<void*>(&SetRenderStateHook),g_set_render_state);
    const bool vs_i=Hook(vtable[96],reinterpret_cast<void*>(&SetVSConstantIHook),g_vs_i);
    const bool vs_b=Hook(vtable[98],reinterpret_cast<void*>(&SetVSConstantBHook),g_vs_b);
    const bool ps_i=Hook(vtable[111],reinterpret_cast<void*>(&SetPSConstantIHook),g_ps_i);
    const bool ps_b=Hook(vtable[113],reinterpret_cast<void*>(&SetPSConstantBHook),g_ps_b);
    const bool freq=Hook(vtable[102],reinterpret_cast<void*>(&SetStreamFrequencyHook),g_freq);
    const bool material=Hook(vtable[49],reinterpret_cast<void*>(&SetMaterialHook),g_set_material);
    const bool light=Hook(vtable[51],reinterpret_cast<void*>(&SetLightHook),g_set_light);
    const bool light_enable=Hook(vtable[53],reinterpret_cast<void*>(&LightEnableHook),g_light_enable);
    const bool clip_plane=Hook(vtable[55],reinterpret_cast<void*>(&SetClipPlaneHook),g_set_clip_plane);
    const bool clip_status=Hook(vtable[62],reinterpret_cast<void*>(&SetClipStatusHook),g_set_clip_status);
    const bool software_vp=Hook(vtable[77],reinterpret_cast<void*>(&SetSoftwareVertexProcessingHook),g_set_software_vp);
    const bool npatch=Hook(vtable[79],reinterpret_cast<void*>(&SetNPatchModeHook),g_set_npatch_mode);
    const bool process_vertices=Hook(vtable[91],reinterpret_cast<void*>(&ProcessVerticesHook),g_process_vertices);
    const bool rect_patch=Hook(vtable[115],reinterpret_cast<void*>(&DrawRectPatchHook),g_draw_rect_patch);
    const bool tri_patch=Hook(vtable[116],reinterpret_cast<void*>(&DrawTriPatchHook),g_draw_tri_patch);
    const bool delete_patch=Hook(vtable[117],reinterpret_cast<void*>(&DeletePatchHook),g_delete_patch);
    // Saved-state Apply can mutate camera/texture/shader without calling
    // the public Set* COM methods. Hook both state-block creation routes.
    const bool create_block=Hook(vtable[59],reinterpret_cast<void*>(&CreateStateBlockHook),g_create_state_block);
    const bool begin_block=Hook(vtable[60],reinterpret_cast<void*>(&BeginStateBlockHook),g_begin_state_block);
    const bool end_block=Hook(vtable[61],reinterpret_cast<void*>(&EndStateBlockHook),g_end_state_block);
    // Scene boundary observation is optional: if either COM hook cannot
    // be installed, rollback BOTH and mark scene source unknown. This
    // must never prevent the normal original game/mono fallback path.
    const bool sb=Hook(
        vtable[41],reinterpret_cast<void*>(&BeginSceneHook),g_begin_scene);
    const bool se=Hook(
        vtable[42],reinterpret_cast<void*>(&EndSceneHook),g_end_scene);
    if(!(sb&&se)){
        if(sb){MH_DisableHook(vtable[41]);MH_RemoveHook(vtable[41]);}
        if(se){MH_DisableHook(vtable[42]);MH_RemoveHook(vtable[42]);}
        g_begin_scene=nullptr;g_end_scene=nullptr;
        m2vr::LogEvent("model2_d3d9_scene_hooks_unavailable",sb,se,0u,0u);
    }
    if(!(p&&cursor_image&&cursor_position&&cursor_visibility&&dialog&&gamma&&create_swap_chain&&get_swap_chain&&validate&&create_query&&dp&&di&&du&&iu&&reset&&clear&&make_tex&&make_volume&&make_cube&&make_vb&&make_ib&&make_rt&&make_depth_surface&&make_offscreen&&get_back&&get_rt&&get_depth&&read_rt&&read_front&&update&&texture_upload&&palette_entries&&current_palette&&stretch&&fill&&rt&&depth&&transform&&multiply_transform&&vs_constants&&viewport&&scissor&&stream_source&&get_stream&&indices&&get_indices&&vertex_declaration&&f_v_f&&vertex_shader&&pixel_shader&&ps_constants&&texture&&get_texture&&texture_stage&&sampler&&render_state&&create_block&&begin_block&&end_block&&vs_i&&vs_b&&ps_i&&ps_b&&freq&&material&&light&&light_enable&&clip_plane&&clip_status&&software_vp&&npatch&&process_vertices&&rect_patch&&tri_patch&&delete_patch&&evict)){
        // Transactional rollback: partial GPU hooks must never be left armed
        // after hook setup fails. Keep the emulator's normal render path.
        if(p){MH_DisableHook(vtable[17]);MH_RemoveHook(vtable[17]);}
        if(cursor_image){MH_DisableHook(vtable[10]);MH_RemoveHook(vtable[10]);}
        if(cursor_position){MH_DisableHook(vtable[11]);MH_RemoveHook(vtable[11]);}
        if(cursor_visibility){MH_DisableHook(vtable[12]);MH_RemoveHook(vtable[12]);}
        if(dialog){MH_DisableHook(vtable[20]);MH_RemoveHook(vtable[20]);}
        if(gamma){MH_DisableHook(vtable[21]);MH_RemoveHook(vtable[21]);}
        if(create_swap_chain){MH_DisableHook(vtable[13]);MH_RemoveHook(vtable[13]);}
        if(get_swap_chain){MH_DisableHook(vtable[14]);MH_RemoveHook(vtable[14]);}
        if(create_query && g_query_hooks_enabled){MH_DisableHook(vtable[118]);MH_RemoveHook(vtable[118]);}
        if(validate){MH_DisableHook(vtable[70]);MH_RemoveHook(vtable[70]);}
        if(evict){MH_DisableHook(vtable[5]);MH_RemoveHook(vtable[5]);}
        if(dp){MH_DisableHook(vtable[81]);MH_RemoveHook(vtable[81]);}
        if(di){MH_DisableHook(vtable[82]);MH_RemoveHook(vtable[82]);}
        if(du){MH_DisableHook(vtable[83]);MH_RemoveHook(vtable[83]);}
        if(iu){MH_DisableHook(vtable[84]);MH_RemoveHook(vtable[84]);}
        if(reset){MH_DisableHook(vtable[16]);MH_RemoveHook(vtable[16]);}
        if(sb&&se){
            MH_DisableHook(vtable[41]);MH_RemoveHook(vtable[41]);
            MH_DisableHook(vtable[42]);MH_RemoveHook(vtable[42]);
        }
        g_begin_scene=nullptr;g_end_scene=nullptr;
        // All four hooks are mandatory in either CPU or GPU mode. Do not
        // strand an enabled trampoline on a partial install failure.
        if(clear){MH_DisableHook(vtable[43]);MH_RemoveHook(vtable[43]);}
        if(make_tex){MH_DisableHook(vtable[23]);MH_RemoveHook(vtable[23]);}
        if(make_volume){MH_DisableHook(vtable[24]);MH_RemoveHook(vtable[24]);}
        if(make_cube){MH_DisableHook(vtable[25]);MH_RemoveHook(vtable[25]);}
        if(make_vb){MH_DisableHook(vtable[26]);MH_RemoveHook(vtable[26]);}
        if(make_ib){MH_DisableHook(vtable[27]);MH_RemoveHook(vtable[27]);}
        if(make_rt){MH_DisableHook(vtable[28]);MH_RemoveHook(vtable[28]);}
        if(make_depth_surface){MH_DisableHook(vtable[29]);MH_RemoveHook(vtable[29]);}
        if(make_offscreen){MH_DisableHook(vtable[36]);MH_RemoveHook(vtable[36]);}
        if(get_back){MH_DisableHook(vtable[18]);MH_RemoveHook(vtable[18]);}
        if(get_rt){MH_DisableHook(vtable[38]);MH_RemoveHook(vtable[38]);}
        if(get_depth){MH_DisableHook(vtable[40]);MH_RemoveHook(vtable[40]);}
        if(read_rt){MH_DisableHook(vtable[32]);MH_RemoveHook(vtable[32]);}
        if(read_front){MH_DisableHook(vtable[33]);MH_RemoveHook(vtable[33]);}
        if(update){MH_DisableHook(vtable[30]);MH_RemoveHook(vtable[30]);}
        if(texture_upload){MH_DisableHook(vtable[31]);MH_RemoveHook(vtable[31]);}
        if(palette_entries){MH_DisableHook(vtable[71]);MH_RemoveHook(vtable[71]);}
        if(current_palette){MH_DisableHook(vtable[73]);MH_RemoveHook(vtable[73]);}
        if(stretch){MH_DisableHook(vtable[34]);MH_RemoveHook(vtable[34]);}
        if(fill){MH_DisableHook(vtable[35]);MH_RemoveHook(vtable[35]);}
        if(rt){MH_DisableHook(vtable[37]);MH_RemoveHook(vtable[37]);}
        if(depth){MH_DisableHook(vtable[39]);MH_RemoveHook(vtable[39]);}
        if(transform){MH_DisableHook(vtable[44]);MH_RemoveHook(vtable[44]);}
        if(multiply_transform){MH_DisableHook(vtable[46]);MH_RemoveHook(vtable[46]);}
        if(vs_constants){MH_DisableHook(vtable[94]);MH_RemoveHook(vtable[94]);}
        if(viewport){MH_DisableHook(vtable[47]);MH_RemoveHook(vtable[47]);}
        if(scissor){MH_DisableHook(vtable[75]);MH_RemoveHook(vtable[75]);}
        if(stream_source){MH_DisableHook(vtable[100]);MH_RemoveHook(vtable[100]);}
        if(get_stream){MH_DisableHook(vtable[101]);MH_RemoveHook(vtable[101]);}
        if(indices){MH_DisableHook(vtable[104]);MH_RemoveHook(vtable[104]);}
        if(get_indices){MH_DisableHook(vtable[105]);MH_RemoveHook(vtable[105]);}
        if(vertex_declaration){MH_DisableHook(vtable[87]);MH_RemoveHook(vtable[87]);}
        if(f_v_f){MH_DisableHook(vtable[89]);MH_RemoveHook(vtable[89]);}
        if(vertex_shader){MH_DisableHook(vtable[92]);MH_RemoveHook(vtable[92]);}
        if(pixel_shader){MH_DisableHook(vtable[107]);MH_RemoveHook(vtable[107]);}
        if(ps_constants){MH_DisableHook(vtable[109]);MH_RemoveHook(vtable[109]);}
        if(texture){MH_DisableHook(vtable[65]);MH_RemoveHook(vtable[65]);}
        if(get_texture){MH_DisableHook(vtable[66]);MH_RemoveHook(vtable[66]);}
        if(texture_stage){MH_DisableHook(vtable[67]);MH_RemoveHook(vtable[67]);}
        if(sampler){MH_DisableHook(vtable[69]);MH_RemoveHook(vtable[69]);}
        if(render_state){MH_DisableHook(vtable[57]);MH_RemoveHook(vtable[57]);}
        if(vs_i){MH_DisableHook(vtable[96]);MH_RemoveHook(vtable[96]);}
        if(vs_b){MH_DisableHook(vtable[98]);MH_RemoveHook(vtable[98]);}
        if(ps_i){MH_DisableHook(vtable[111]);MH_RemoveHook(vtable[111]);}
        if(ps_b){MH_DisableHook(vtable[113]);MH_RemoveHook(vtable[113]);}
        if(freq){MH_DisableHook(vtable[102]);MH_RemoveHook(vtable[102]);}
        if(material){MH_DisableHook(vtable[49]);MH_RemoveHook(vtable[49]);}
        if(light){MH_DisableHook(vtable[51]);MH_RemoveHook(vtable[51]);}
        if(light_enable){MH_DisableHook(vtable[53]);MH_RemoveHook(vtable[53]);}
        if(clip_plane){MH_DisableHook(vtable[55]);MH_RemoveHook(vtable[55]);}
        if(clip_status){MH_DisableHook(vtable[62]);MH_RemoveHook(vtable[62]);}
        if(software_vp){MH_DisableHook(vtable[77]);MH_RemoveHook(vtable[77]);}
        if(npatch){MH_DisableHook(vtable[79]);MH_RemoveHook(vtable[79]);}
        if(process_vertices){MH_DisableHook(vtable[91]);MH_RemoveHook(vtable[91]);}
        if(rect_patch){MH_DisableHook(vtable[115]);MH_RemoveHook(vtable[115]);}
        if(tri_patch){MH_DisableHook(vtable[116]);MH_RemoveHook(vtable[116]);}
        if(delete_patch){MH_DisableHook(vtable[117]);MH_RemoveHook(vtable[117]);}
        if(create_block){MH_DisableHook(vtable[59]);MH_RemoveHook(vtable[59]);}
        if(begin_block){MH_DisableHook(vtable[60]);MH_RemoveHook(vtable[60]);}
        if(end_block){MH_DisableHook(vtable[61]);MH_RemoveHook(vtable[61]);}
        g_present=nullptr;g_draw=nullptr;g_draw_indexed=nullptr;
        g_original_set_cursor_properties=nullptr;g_original_set_cursor_position=nullptr;
        g_original_show_cursor=nullptr;
        g_original_set_dialog_box_mode=nullptr;g_original_set_gamma_ramp=nullptr;
        g_create_additional_swap_chain=nullptr;g_get_swap_chain=nullptr;
        g_create_query=nullptr;g_validate_device=nullptr;
        g_evict_managed_resources=nullptr;
        g_reset=nullptr;g_clear=nullptr;
        g_create_vertex_buffer=nullptr;g_create_index_buffer=nullptr;
        g_create_texture=nullptr;g_create_volume_texture=nullptr;
        g_create_cube_texture=nullptr;
        g_create_render_target=nullptr;g_create_depth_stencil_surface=nullptr;
        g_create_offscreen_plain_surface=nullptr;
        g_get_back_buffer=nullptr;g_get_render_target=nullptr;
        g_get_depth_stencil_surface=nullptr;
        g_get_render_target_data=nullptr;g_get_front_buffer_data=nullptr;
        g_update_surface=nullptr;g_update_texture=nullptr;
        g_set_palette_entries=nullptr;g_set_current_texture_palette=nullptr;
        g_stretch_rect=nullptr;g_color_fill=nullptr;
        g_set_render_target=nullptr;g_set_depth_surface=nullptr;
        g_set_transform=nullptr;g_multiply_transform=nullptr;
        g_set_vs_constant_f=nullptr;
        g_set_viewport=nullptr;g_set_scissor=nullptr;
        g_set_stream_source=nullptr;g_get_stream_source=nullptr;
        g_set_indices=nullptr;g_get_indices=nullptr;
        g_set_vertex_declaration=nullptr;
        g_set_f_v_f=nullptr;
        g_set_vertex_shader=nullptr;
        g_set_pixel_shader=nullptr;
        g_set_ps_constant_f=nullptr;
        g_set_texture=nullptr;g_get_texture=nullptr;g_set_texture_stage=nullptr;
        g_set_sampler_state=nullptr;g_set_render_state=nullptr;
        g_vs_i=nullptr;
        g_vs_b=nullptr;
        g_ps_i=nullptr;
        g_ps_b=nullptr;
        g_freq=nullptr;
        g_set_material=nullptr;g_set_light=nullptr;
        g_light_enable=nullptr;g_set_clip_plane=nullptr;
        g_set_clip_status=nullptr;g_set_software_vp=nullptr;g_set_npatch_mode=nullptr;
        g_process_vertices=nullptr;g_draw_rect_patch=nullptr;
        g_draw_tri_patch=nullptr;g_delete_patch=nullptr;
        g_create_state_block=nullptr;g_begin_state_block=nullptr;
        g_end_state_block=nullptr;
        g_draw_up=nullptr;g_draw_indexed_up=nullptr;
        m2vr::LogEvent("model2_d3d9_partial_hook_rolled_back",p,dp,di,du);
        m2vr::LogEvent("model2_d3d9_original_presentation_hook_rollback",
            dialog,gamma,0u,0u);
        m2vr::LogEvent("model2_d3d9_original_swap_chain_hook_rollback",
            create_swap_chain,get_swap_chain,0u,0u);
        m2vr::LogEvent("model2_d3d9_original_validation_hook_rollback",validate,0u,0u,0u);
        m2vr::LogEvent("model2_d3d9_surface_hook_rollback",
                       update,stretch,fill,clear);
        m2vr::LogEvent("model2_d3d9_buffer_create_hook_rollback",
                        make_vb,make_ib,0u,0u);
        m2vr::LogEvent("model2_d3d9_texture_create_hook_rollback",
                        make_tex,make_volume,make_cube,0u);
        m2vr::LogEvent("model2_d3d9_surface_create_hook_rollback",
                        make_rt,make_depth_surface,make_offscreen,0u);
        m2vr::LogEvent("model2_d3d9_surface_readback_hook_rollback",
                        read_rt,read_front,0u,0u);
        m2vr::LogEvent("model2_d3d9_surface_getter_hook_rollback",
                        get_back,get_rt,get_depth,0u);
        m2vr::LogEvent("model2_d3d9_texture_upload_hook_rollback",
                       texture_upload,0u,0u,0u);
        m2vr::LogEvent("model2_d3d9_palette_hooks_rollback",
                       palette_entries,current_palette,0u,0u);
        m2vr::LogEvent("model2_d3d9_target_hook_rollback",rt,depth,0u,0u);
        m2vr::LogEvent("model2_d3d9_transform_hook_rollback",
            transform,multiply_transform,vs_constants,0u);
        m2vr::LogEvent("model2_d3d9_region_hook_rollback",viewport,scissor,0u,0u);
        m2vr::LogEvent("model2_d3d9_pipeline_hook_rollback",
            stream_source,indices,vertex_declaration,f_v_f);
        m2vr::LogEvent("model2_d3d9_resource_getter_hook_rollback",
            get_stream,get_indices,get_texture,0u);
        m2vr::LogEvent("model2_d3d9_shader_hook_rollback",
            vertex_shader,pixel_shader,ps_constants,0u);
        m2vr::LogEvent("model2_d3d9_material_hook_rollback",
            texture,texture_stage,sampler,render_state);
        m2vr::LogEvent("model2_d3d9_aux_shader_hook_rollback",
            vs_i,vs_b,ps_i,ps_b);
        m2vr::LogEvent("model2_d3d9_stream_freq_hook_rollback",
            freq,0u,0u,0u);
        m2vr::LogEvent("model2_d3d9_fixed_function_hook_rollback",
            material,light,light_enable,clip_plane);
        m2vr::LogEvent("model2_d3d9_raster_mode_hook_rollback",clip_status,software_vp,npatch,0u);
        m2vr::LogEvent("model2_d3d9_unmirrored_geometry_hook_rollback",
            process_vertices,rect_patch,tri_patch,delete_patch);
        m2vr::LogEvent("model2_d3d9_stateblock_hook_rollback",
            create_block,begin_block,end_block,0u);
        return false;
    }
    // Vtable methods are shared by D3D9 devices. Bind the real first
    // successfully hooked game device before declaring the hook ready.
    if(!g_primary_device.BindOnce(dev))return false;
    g_original_scene_hooks_ready.store(sb&&se,std::memory_order_release);
    g_device_installed.store(true,std::memory_order_release);
    m2vr::LogEvent("model2_d3d9_device_hooks_ready");
    return true;
}
HRESULT STDMETHODCALLTYPE CreateDeviceHook(IDirect3D9* api,UINT adapter,D3DDEVTYPE type,
    HWND hwnd,DWORD flags,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice9** out){
    const HRESULT hr=g_create_device(api,adapter,type,hwnd,flags,pp,out);
    if(SUCCEEDED(hr)&&out&&*out){
        m2vr::LogEvent("model2_d3d9_device_created",
            pp?pp->BackBufferWidth:0,pp?pp->BackBufferHeight:0,flags);
        if(pp){
            m2vr::LogEvent("model2_d3d9_device_surface",
                static_cast<std::uint32_t>(pp->BackBufferFormat),
                static_cast<std::uint32_t>(pp->MultiSampleType),
                static_cast<std::uint32_t>(pp->AutoDepthStencilFormat),
                pp->EnableAutoDepthStencil?1u:0u);
        }
        const bool hooks_ready=HookDevice(*out);
        m2vr::LogEvent("model2_d3d9_device_hook_result",hooks_ready?1u:0u);
        // This is a one-time startup boundary. Flush it immediately so a
        // later device/render failure cannot hide the exact hook outcome.
        m2vr::FlushLog();
    }
    return hr;
}
IDirect3D9* WINAPI Create9Hook(UINT sdk){
    IDirect3D9* api=g_create9(sdk);
    if(api && !g_create_device){
        // IDirect3D9::CreateDevice is method 16.
        auto** vtable=*reinterpret_cast<void***>(api);
        std::lock_guard lock(g_setup_mutex);
        if(!g_create_device){
            if(!Hook(vtable[16],reinterpret_cast<void*>(&CreateDeviceHook),g_create_device))
                m2vr::LogEvent("model2_d3d9_create_device_hook_failed");
        }
    }
    return api;
}
} // namespace

// The authenticated source adapter must use this specific original D3D9
// frame identifier instead of assuming its emulator simulation frame index
// equals a real device Present. Only an independently authenticated world
// transaction can replace experimental GPU replay on that physical frame.
extern "C" __declspec(dllexport) BOOL __stdcall
Model2VR_IsOriginalPrimaryD3D9Device(const void* device){
    return g_device_installed.load(std::memory_order_acquire) &&
           g_primary_device.Owns(device) ? TRUE : FALSE;
}
extern "C" __declspec(dllexport) unsigned long long __stdcall
Model2VR_NextOriginalD3D9DrawOrdinal(){
    // A source ordinal is meaningful only inside the genuine EXE-owned
    // successful primary BeginScene, on the original render thread.
    // Do not let an asynchronous/overlay caller arm a future game Draw.
    if(!g_device_installed.load(std::memory_order_acquire) ||
       !Model2VR_CurrentOriginalD3D9SceneOrdinal())return 0u;
    const auto prior=g_original_frame_draw_ordinal.load(
        std::memory_order_acquire);
    return prior<10000000u ? prior+1u : 0u;
}
extern "C" __declspec(dllexport) unsigned int __stdcall
Model2VR_CurrentOriginalD3D9SceneOrdinal(){
    if(!g_device_installed.load(std::memory_order_acquire) ||
       !g_original_scene_hooks_ready.load(std::memory_order_acquire) ||
       g_original_scene_poisoned.load(std::memory_order_acquire) ||
       !g_original_scene_open.load(std::memory_order_acquire) ||
       g_original_scene_owner_thread.load(std::memory_order_acquire)!=
           GetCurrentThreadId())return 0u;
    return g_original_scene_ordinal.load(std::memory_order_acquire);
}
extern "C" __declspec(dllexport) unsigned long long __stdcall
Model2VR_NextOriginalD3D9PresentFrameId(){
    return m2vr::vrbridge::NextOriginalPresentFrameId(
        g_present_count.load(std::memory_order_relaxed),
        g_device_installed.load(std::memory_order_acquire),
        g_native_stereo);
}

// Read-only current-Present state checksum, on the authentic original game
// scene owner thread only. Does not grant World3D or stereo by itself.
extern "C" __declspec(dllexport) unsigned long long __stdcall
Model2VR_CurrentOriginalD3D9LineageNonce(){
    if(g_state_block_apply_unobserved.load(std::memory_order_acquire) ||
       g_original_stream_frequency_state.UnsupportedMask() ||
       g_original_raster_mode_state.UnsupportedMask() ||
       g_original_validation_unproven.load(std::memory_order_acquire) ||
       g_unmirrored_geometry_state.HasProcessedVertices() ||
       g_buffer_hook_unknown.load(std::memory_order_acquire) ||
       g_original_bound_write_locks.HasOutstandingWrites() ||
       g_texture_hook_unknown.load(std::memory_order_acquire) ||
       g_original_texture_write_locks.HasOutstandingWrites() ||
       !Model2VR_CurrentOriginalD3D9SceneOrdinal())return 0u;
    return m2vr::drawprobe::OriginalLineageNonce(ReadOriginalLineage(
        Model2VR_NextOriginalD3D9PresentFrameId()));
}

namespace m2vr {
bool InstallD3D9ReadOnlyProbe(HMODULE executable,std::uint32_t exe_size,
                             std::uint32_t draw_sample_every,
                             bool native_stereo,std::uint32_t eye_shift_milliunits) {
    if(!executable || exe_size==0)return false;
    g_exe_base=reinterpret_cast<std::uintptr_t>(executable);
    g_exe_size=exe_size;
    // A diagnostic setting is an integer [64,4096] after INI parsing.
    // Preserve conservative sampling if this function is called directly.
    char query_opt_in[8]{};
    // Disabled unless the user deliberately requests a diagnostic A/B.
    // Unobserved original GPU Queries cannot certify an XR GameStereo eye.
    const DWORD opt_in_size=GetEnvironmentVariableA(
        "MODEL2VR_EXPERIMENTAL_QUERY_HOOKS",query_opt_in,sizeof(query_opt_in));
    g_query_hooks_enabled=opt_in_size==1u && query_opt_in[0]=='1';
    g_native_stereo=native_stereo && g_query_hooks_enabled;
    m2vr::daytona::Configure(g_native_stereo,eye_shift_milliunits);
    LogEvent("model2_d3d9_query_startup_safe_mode",
        g_query_hooks_enabled?1u:0u,native_stereo?1u:0u,
        g_native_stereo?1u:0u,0u);
    FlushLog();
    g_draw_sample_every=(draw_sample_every>=64u &&
        draw_sample_every<=4096u)?draw_sample_every:1024u;
    LogEvent("model2_d3d9_draw_interval",g_draw_sample_every);
    HMODULE d3d9=GetModuleHandleW(L"d3d9.dll");
    if(!d3d9) {
        // The loader injects before the suspended emulator main thread
        // resumes, so d3d9.dll commonly has not been loaded yet.
        // Load the process's normal D3D9 runtime *without creating a GPU
        // device* so the original Direct3DCreate9 call can be hooked.
        // The reference stays loaded for process-lifetime probe hooks.
        d3d9=LoadLibraryW(L"d3d9.dll");
        if(d3d9)LogEvent("model2_d3d9_runtime_preloaded");
    }
    if(!d3d9){
        LogEvent("model2_d3d9_runtime_load_failed",GetLastError());
        return false;
    }
    auto* create=reinterpret_cast<void*>(GetProcAddress(d3d9,"Direct3DCreate9"));
    if(!create)return false;
    const MH_STATUS status=MH_CreateHook(create,
        reinterpret_cast<void*>(&Create9Hook),reinterpret_cast<LPVOID*>(&g_create9));
    if(status!=MH_OK){LogEvent("model2_d3d9_create9_hook_failed",status);return false;}
    // The main probe installer enables all hooks atomically after signatures
    // pass. Runtime-created device hooks are enabled when discovered.
    LogEvent("model2_d3d9_create9_hook_armed");
    return true;
}
} // namespace m2vr