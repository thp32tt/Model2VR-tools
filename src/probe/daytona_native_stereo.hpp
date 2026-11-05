#pragma once
// Opt-in Daytona D3D9 true per-eye GPU replay. This is independent of
// CPU simulation, input and the user's FFB plugin. Experimental, not
// calibrated six-DoF until original view matrices are recovered.
#include <Windows.h>
#include <d3d9.h>
#include <cstdint>

namespace m2vr::daytona {
using OriginalDrawPrimitive=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
using OriginalDrawIndexed=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
using OriginalDrawIndexedUP=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,
    D3DFORMAT,const void*,UINT);
using OriginalClear=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD);
using OriginalUpdateSurface=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const POINT*);
using OriginalStretchRect=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,
    const RECT*,D3DTEXTUREFILTERTYPE);
using OriginalColorFill=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,D3DCOLOR);
using OriginalDrawUP=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
void Configure(bool enabled,std::uint32_t shift_milliunits) noexcept;
void MirrorDraw(IDirect3DDevice9* device,D3DPRIMITIVETYPE primitive,
                UINT count,const void* data,UINT stride,
                std::uint32_t caller_rva,OriginalDrawUP original) noexcept;
// Mirror only successfully completed original draws into independent eyes.
// No second emulator simulation/input/FFB path and no game draw suppression.
void MirrorPrimitive(IDirect3DDevice9* device,D3DPRIMITIVETYPE primitive,
                     UINT start,UINT count,OriginalDrawPrimitive original) noexcept;
void MirrorIndexed(IDirect3DDevice9* device,D3DPRIMITIVETYPE primitive,
                   INT base,UINT min,UINT num,UINT start,UINT count,
                   OriginalDrawIndexed original) noexcept;
void MirrorIndexedUP(IDirect3DDevice9* device,D3DPRIMITIVETYPE primitive,
                     UINT min,UINT num,UINT count,const void* indices,
                     D3DFORMAT format,const void* vertices,UINT stride,
                     OriginalDrawIndexedUP original) noexcept;
// Preserve source clear colour, Z, stencil and partial clear rectangles.
void MirrorClear(IDirect3DDevice9* device,DWORD count,const D3DRECT* rects,
                 DWORD flags,D3DCOLOR color,float z,DWORD stencil,
                 OriginalClear original) noexcept;
// A single verified full-size offscreen source RT may also be mirrored
// into independent eye RT/DS surfaces and passed through StretchRect.
// Unsupported target/format/partial initialization stays fail-closed.
// Preserve completed original backbuffer surface writes in eye order;
 // initial partial writes and backbuffer self-blits are fail-closed.
void MirrorUpdateSurface(IDirect3DDevice9* device,IDirect3DSurface9* source,
                         const RECT* source_rect,IDirect3DSurface9* destination,
                         const POINT* destination_point,
                         OriginalUpdateSurface original) noexcept;
void MirrorStretchRect(IDirect3DDevice9* device,IDirect3DSurface9* source,
                       const RECT* source_rect,IDirect3DSurface9* destination,
                       const RECT* destination_rect,D3DTEXTUREFILTERTYPE filter,
                       OriginalStretchRect original) noexcept;
void MirrorColorFill(IDirect3DDevice9* device,IDirect3DSurface9* destination,
                     const RECT* rect,D3DCOLOR color,
                     OriginalColorFill original) noexcept;
// Preserve original backbuffer contents by staging before Present; delay
// x86/x64 GameStereo publication and final health until original HRESULT.
void PrepareFrameBeforePresent(IDirect3DDevice9* device,
                               std::uint64_t frame,
                               bool verified_cpu_world_pending=false) noexcept;
void FinishFrameAfterPresent(std::uint64_t frame,
                             HRESULT original_result) noexcept;
// Drop only the matching staged experimental GPU eye data when the
// authenticated CPU-world frame already published after original Present.
void DiscardPreparedFrameAfterCpuWorld(std::uint64_t frame) noexcept;
// Unified original-game source health writer. The authenticated CPU world
// reports through this single mapping; GPU replay keeps backup ownership.
void ReportVerifiedCpuWorldPresent(std::uint64_t frame,
                                   std::uint32_t reason,
                                   std::uint32_t draw_count) noexcept;
// An authentic primary-device game EXE BeginScene/EndScene HRESULT failure
// revokes this original Present's speculative GPU eyes. The one unchanged
// original backbuffer remains eligible for clearly tagged MONO fallback.
void MarkFailedOriginalScene(std::uint32_t transition,
                             std::uint32_t original_hresult) noexcept;
// A failed original EXE-owned primary Clear revokes incomplete GPU eyes.
void MarkFailedOriginalClear(std::uint32_t flags,
                              std::uint32_t original_hresult) noexcept;
// A failed primary-device game EXE Draw in any of four families invalidates
// speculative GPU eyes, but never changes the original Draw HRESULT.
void MarkFailedOriginalDraw(std::uint32_t family,
                            std::uint32_t original_hresult) noexcept;
void ResetResources() noexcept;
}
