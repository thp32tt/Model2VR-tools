// Original D3D9 cube / volume texture Lock methods (vtable slots 19/20).
// These ABI signatures differ from a 2D texture. Do not capture/log pixels.
void OriginalTextureCoverageUnknown(std::uint32_t caller_rva) noexcept;
HRESULT STDMETHODCALLTYPE OriginalCubeLockRectHook(IDirect3DCubeTexture9* t,
    D3DCUBEMAP_FACES face,UINT level,D3DLOCKED_RECT* locked,
    const RECT* rect,DWORD flags){
    if(!g_cube_lock_rect)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_cube_lock_rect(t,face,level,locked,rect,flags);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,43u,rva);
    else if(!m2vr::drawprobe::IsWritableBufferLock(flags) &&
            g_original_texture_write_locks.Contains(t))
        OriginalTextureCoverageUnknown(rva);
    else if(m2vr::drawprobe::IsWritableBufferLock(flags)){
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::CubeTextureContent,rva,hr);
        g_original_texture_shape_writes[0].fetch_add(1u,std::memory_order_relaxed);
        if(!g_original_texture_write_locks.Begin(t))OriginalTextureCoverageUnknown(rva);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCubeUnlockRectHook(IDirect3DCubeTexture9* t,
    D3DCUBEMAP_FACES face,UINT level){
    if(!g_cube_unlock_rect)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_cube_unlock_rect(t,face,level);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))g_original_texture_write_locks.SuccessfulUnlock(t);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,44u,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeLockBoxHook(IDirect3DVolumeTexture9* t,
    UINT level,D3DLOCKED_BOX* locked,const D3DBOX* box,DWORD flags){
    if(!g_volume_lock_box)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_lock_box(t,level,locked,box,flags);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,45u,rva);
    else if(!m2vr::drawprobe::IsWritableBufferLock(flags) &&
            g_original_texture_write_locks.Contains(t))
        OriginalTextureCoverageUnknown(rva);
    else if(m2vr::drawprobe::IsWritableBufferLock(flags)){
        RecordOriginalLineageWrite(m2vr::drawprobe::OriginalLineageKind::VolumeTextureContent,rva,hr);
        g_original_texture_shape_writes[1].fetch_add(1u,std::memory_order_relaxed);
        if(!g_original_texture_write_locks.Begin(t))OriginalTextureCoverageUnknown(rva);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeUnlockBoxHook(IDirect3DVolumeTexture9* t,
    UINT level){
    if(!g_volume_unlock_box)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_unlock_box(t,level);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))g_original_texture_write_locks.SuccessfulUnlock(t);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,46u,rva);
    return hr;
}
bool ObserveOriginalBoundShapeTexture(IDirect3DBaseTexture9* t,
    std::uint32_t rva,D3DRESOURCETYPE shape){
    if(!t || !rva)return true;
    if(shape!=D3DRTYPE_CUBETEXTURE && shape!=D3DRTYPE_VOLUMETEXTURE){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    std::lock_guard lock(g_texture_hook_mutex);
    if(g_texture_hook_unknown.load(std::memory_order_acquire))return false;
    auto** vt=*reinterpret_cast<void***>(t);
    if(!vt || !vt[19] || !vt[20]){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    const bool cube=shape==D3DRTYPE_CUBETEXTURE;
    void*& lock_target=cube?g_cube_lock_target:g_volume_lock_target;
    void*& unlock_target=cube?g_cube_unlock_target:g_volume_unlock_target;
    if(vt[19]==g_texture_lock_target || vt[20]==g_texture_unlock_target ||
       vt[19]==(cube?g_volume_lock_target:g_cube_lock_target) ||
       vt[20]==(cube?g_volume_unlock_target:g_cube_unlock_target)){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(lock_target || unlock_target){
        const bool complete=cube?(g_cube_lock_rect && g_cube_unlock_rect):
                                 (g_volume_lock_box && g_volume_unlock_box);
        if(complete && lock_target==vt[19] && unlock_target==vt[20])return true;
        OriginalTextureCoverageUnknown(rva);return false;
    }
    const bool h1=cube?
        Hook(vt[19],reinterpret_cast<void*>(&OriginalCubeLockRectHook),g_cube_lock_rect):
        Hook(vt[19],reinterpret_cast<void*>(&OriginalVolumeLockBoxHook),g_volume_lock_box);
    if(!h1){OriginalTextureCoverageUnknown(rva);return false;}
    const bool h2=cube?
        Hook(vt[20],reinterpret_cast<void*>(&OriginalCubeUnlockRectHook),g_cube_unlock_rect):
        Hook(vt[20],reinterpret_cast<void*>(&OriginalVolumeUnlockBoxHook),g_volume_unlock_box);
    if(!h2){
        MH_DisableHook(vt[19]);MH_RemoveHook(vt[19]);
        if(cube)g_cube_lock_rect=nullptr;
        else g_volume_lock_box=nullptr;
        OriginalTextureCoverageUnknown(rva);return false;
    }
    lock_target=vt[19];unlock_target=vt[20];
    return true;
}
