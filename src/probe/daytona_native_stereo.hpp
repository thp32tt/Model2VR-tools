#pragma once
// Opt-in Daytona D3D9 true per-eye GPU replay. This is independent of
// CPU simulation, input and the user's FFB plugin. Experimental, not
// calibrated six-DoF until original view matrices are recovered.
#include <Windows.h>
#include <d3d9.h>
#include <cstdint>

namespace m2vr::daytona {
using OriginalDrawUP=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
void Configure(bool enabled,std::uint32_t shift_milliunits) noexcept;
void MirrorDraw(IDirect3DDevice9* device,D3DPRIMITIVETYPE primitive,
                UINT count,const void* data,UINT stride,
                std::uint32_t caller_rva,OriginalDrawUP original) noexcept;
void PublishFrame(IDirect3DDevice9* device,std::uint64_t frame) noexcept;
void ResetResources() noexcept;
}
