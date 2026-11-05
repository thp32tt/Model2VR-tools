// Original bound cube Surface and volume level: real source COM once,
// HRESULT and original mono untouched. No copied/logged game texels or pointers.
bool PrimaryOriginalVolume(IDirect3DVolume9* v) noexcept {
    if(!v)return false;
    IDirect3DDevice9* d=nullptr;
    const HRESULT hr=v->GetDevice(&d);
    const bool primary=SUCCEEDED(hr)&&d&&g_primary_device.Owns(d);
    if(d)d->Release();
    return primary;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeLevelLockBoxHook(IDirect3DVolume9* v,
    D3DLOCKED_BOX* data,const D3DBOX* box,DWORD flags){
    if(!g_volume_level_lock)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalVolume(v);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_level_lock(v,data,box,flags);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,54u,rva);
    else if(!m2vr::drawprobe::IsWritableBufferLock(flags) &&
            g_original_texture_write_locks.Contains(v))
        OriginalTextureCoverageUnknown(rva);
    else if(m2vr::drawprobe::IsWritableBufferLock(flags)){
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::VolumeTextureContent,rva,hr);
        g_original_texture_shape_writes[1].fetch_add(1u,std::memory_order_relaxed);
        if(!g_original_texture_write_locks.Begin(v))
            OriginalTextureCoverageUnknown(rva);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeLevelUnlockBoxHook(IDirect3DVolume9* v){
    if(!g_volume_level_unlock)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalVolume(v);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_level_unlock(v);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))g_original_texture_write_locks.SuccessfulUnlock(v);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,55u,rva);
    return hr;
}
bool ObserveOriginalVolumeLevel(IDirect3DVolume9* v,std::uint32_t rva){
    if(!v || !rva)return false;
    std::lock_guard lock(g_volume_level_hook_mutex);
    if(g_texture_hook_unknown.load(std::memory_order_acquire))return false;
    auto** vt=*reinterpret_cast<void***>(v);
    // IDirect3DVolume9 inherits IUnknown, unlike IDirect3DResource9.
    if(!vt || !vt[9] || !vt[10]){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(g_volume_level_lock_target || g_volume_level_unlock_target){
        if(g_volume_level_lock_target==vt[9] &&
           g_volume_level_unlock_target==vt[10] &&
           g_volume_level_lock && g_volume_level_unlock)return true;
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(!Hook(vt[9],reinterpret_cast<void*>(&OriginalVolumeLevelLockBoxHook),
             g_volume_level_lock)){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(!Hook(vt[10],reinterpret_cast<void*>(&OriginalVolumeLevelUnlockBoxHook),
             g_volume_level_unlock)){
        MH_DisableHook(vt[9]);MH_RemoveHook(vt[9]);
        g_volume_level_lock=nullptr;
        OriginalTextureCoverageUnknown(rva);return false;
    }
    g_volume_level_lock_target=vt[9];
    g_volume_level_unlock_target=vt[10];
    return true;
}
HRESULT STDMETHODCALLTYPE OriginalCubeGetSurfaceHook(IDirect3DCubeTexture9* t,
    D3DCUBEMAP_FACES face,UINT level,IDirect3DSurface9** out){
    if(!g_cube_get_surface)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_cube_get_surface(t,face,level,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,52u,rva);
    else if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
        OriginalTextureCoverageUnknown(rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeGetLevelHook(IDirect3DVolumeTexture9* t,
    UINT level,IDirect3DVolume9** out){
    if(!g_volume_get_level)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_get_level(t,level,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,53u,rva);
    else if(!out || !*out || !ObserveOriginalVolumeLevel(*out,rva))
        OriginalTextureCoverageUnknown(rva);
    return hr;
}
