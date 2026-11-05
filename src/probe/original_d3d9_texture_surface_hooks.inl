// Bound original 2D texture GetSurfaceLevel -> Surface LockRect / GetDC.
// Every real COM executes exactly once with its unaltered HRESULT.
// Never copy or publish pixels, mip level, rect, DC handles or pointers.
void OriginalTextureCoverageUnknown(std::uint32_t caller_rva) noexcept;
HRESULT STDMETHODCALLTYPE OriginalSurfaceLockRectHook(IDirect3DSurface9* surf,
    D3DLOCKED_RECT* locked,const RECT* region,DWORD flags){
    if(!g_surface_lock_rect)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(surf);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_surface_lock_rect(surf,locked,region,flags);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,48u,rva);
    else if(!m2vr::drawprobe::IsWritableBufferLock(flags) &&
            g_original_texture_write_locks.Contains(surf))
        OriginalTextureCoverageUnknown(rva);
    else if(m2vr::drawprobe::IsWritableBufferLock(flags)){
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::SurfaceTexelContent,rva,hr);
        g_original_texture_surface_writes.fetch_add(1u,std::memory_order_relaxed);
        if(!g_original_texture_write_locks.Begin(surf))
            OriginalTextureCoverageUnknown(rva);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalSurfaceUnlockRectHook(IDirect3DSurface9* surf){
    if(!g_surface_unlock_rect)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(surf);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_surface_unlock_rect(surf);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))
        g_original_texture_write_locks.SuccessfulUnlock(surf);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,49u,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalSurfaceGetDCHook(IDirect3DSurface9* surf,
    HDC* dc){
    if(!g_surface_get_dc)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(surf);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_surface_get_dc(surf,dc);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,50u,rva);
    else if(!dc || !*dc)OriginalTextureCoverageUnknown(rva);
    else {
        // A successful GDI DC can mutate the surface without D3D LockRect.
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::SurfaceTexelContent,rva,hr);
        g_original_texture_surface_writes.fetch_add(1u,std::memory_order_relaxed);
        if(!g_original_texture_write_locks.Begin(surf))
            OriginalTextureCoverageUnknown(rva);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalSurfaceReleaseDCHook(IDirect3DSurface9* surf,
    HDC dc){
    if(!g_surface_release_dc)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(surf);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_surface_release_dc(surf,dc);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))
        g_original_texture_write_locks.SuccessfulUnlock(surf);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,51u,rva);
    return hr;
}
bool ObserveOriginalTextureSurface(IDirect3DSurface9* surf,
                                   std::uint32_t caller_rva){
    if(!surf || !caller_rva)return false;
    if(!EnsureOriginalResourceResidencyObserved(surf,caller_rva)){
        OriginalTextureCoverageUnknown(caller_rva);
        return false;
    }
    std::lock_guard lock(g_texture_surface_hook_mutex);
    if(g_texture_hook_unknown.load(std::memory_order_acquire))return false;
    auto** vt=*reinterpret_cast<void***>(surf);
    for(unsigned slot: {11u,13u,14u,15u,16u}){
        if(!vt || !vt[slot]){
            OriginalTextureCoverageUnknown(caller_rva);return false;
        }
    }
    if(g_surface_container_target || g_surface_lock_target ||
       g_surface_unlock_target || g_surface_get_dc_target ||
       g_surface_release_dc_target){
        if(g_surface_container_target==vt[11] &&
           g_surface_get_container &&
           g_surface_lock_target==vt[13] && g_surface_unlock_target==vt[14] &&
           g_surface_get_dc_target==vt[15] && g_surface_release_dc_target==vt[16] &&
           g_surface_lock_rect && g_surface_unlock_rect &&
           g_surface_get_dc && g_surface_release_dc)return true;
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    const bool h0=Hook(vt[11],
        reinterpret_cast<void*>(&OriginalSurfaceGetContainerHook),
        g_surface_get_container);
    const bool h1=h0&&Hook(vt[13],reinterpret_cast<void*>(&OriginalSurfaceLockRectHook),
                       g_surface_lock_rect);
    const bool h2=h1&&Hook(vt[14],
        reinterpret_cast<void*>(&OriginalSurfaceUnlockRectHook),g_surface_unlock_rect);
    const bool h3=h2&&Hook(vt[15],
        reinterpret_cast<void*>(&OriginalSurfaceGetDCHook),g_surface_get_dc);
    const bool h4=h3&&Hook(vt[16],
        reinterpret_cast<void*>(&OriginalSurfaceReleaseDCHook),g_surface_release_dc);
    if(!(h0&&h1&&h2&&h3&&h4)){
        if(h4){MH_DisableHook(vt[16]);MH_RemoveHook(vt[16]);}
        if(h3){MH_DisableHook(vt[15]);MH_RemoveHook(vt[15]);}
        if(h2){MH_DisableHook(vt[14]);MH_RemoveHook(vt[14]);}
        if(h1){MH_DisableHook(vt[13]);MH_RemoveHook(vt[13]);}
        if(h0){MH_DisableHook(vt[11]);MH_RemoveHook(vt[11]);}
        g_surface_get_container=nullptr;
        g_surface_lock_rect=nullptr;g_surface_unlock_rect=nullptr;
        g_surface_get_dc=nullptr;g_surface_release_dc=nullptr;
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    g_surface_container_target=vt[11];
    g_surface_lock_target=vt[13];g_surface_unlock_target=vt[14];
    g_surface_get_dc_target=vt[15];g_surface_release_dc_target=vt[16];
    return true;
}
HRESULT STDMETHODCALLTYPE OriginalTextureGetSurfaceLevelHook(IDirect3DTexture9* tex,
    UINT level,IDirect3DSurface9** out){
    if(!g_texture_get_surface_level)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(tex);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_texture_get_surface_level(tex,level,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,47u,rva);
    else if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
        OriginalTextureCoverageUnknown(rva);
    return hr;
}
