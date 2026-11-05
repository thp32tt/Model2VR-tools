// Authentic original x86 IDirect3DTexture9::LockRect/UnlockRect tracking.
// Observes anonymous source revisions only; NEVER copies or logs texel data.
void OriginalTextureCoverageUnknown(std::uint32_t caller_rva) noexcept {
    g_texture_hook_unknown.store(true,std::memory_order_release);
    Model2VR_RejectOriginalD3D9BufferHookCoverage(
        g_present_count.load(std::memory_order_acquire)+1u,0u,0u);
    if(caller_rva)m2vr::LogEvent("model2_d3d9_original_texture_hook_diagnostic",
        g_present_count.load(std::memory_order_relaxed)+1u,caller_rva,0u,0u);
}
HRESULT STDMETHODCALLTYPE OriginalTextureLockRectHook(IDirect3DTexture9* texture,
    UINT level,D3DLOCKED_RECT* locked,const RECT* region,DWORD flags) {
    if(!g_texture_lock_rect)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(texture);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_texture_lock_rect(texture,level,locked,region,flags);
    if(!primary)return hr;
    const auto rva=CallerRva(caller);
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,41u,rva);
    else if(!m2vr::drawprobe::IsWritableBufferLock(flags) &&
            g_original_texture_write_locks.Contains(texture))
        OriginalTextureCoverageUnknown(rva);
    else if(m2vr::drawprobe::IsWritableBufferLock(flags)){
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::TextureLockContent,rva,hr);
        g_original_texture_lock_writes.fetch_add(1u,std::memory_order_relaxed);
        if(!g_original_texture_write_locks.Begin(texture))
            OriginalTextureCoverageUnknown(rva);
        // Writer has mapped original pixels: invalidate previously staged eyes.
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalTextureUnlockRectHook(IDirect3DTexture9* texture,
    UINT level){
    if(!g_texture_unlock_rect)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(texture);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_texture_unlock_rect(texture,level);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))
        g_original_texture_write_locks.SuccessfulUnlock(texture);
    if(rva && FAILED(hr))
        TrackFailedOriginalD3D9Surface(hr,42u,rva);
    return hr;
}
bool ObserveOriginalBoundTexture(IDirect3DBaseTexture9* texture,
                                 std::uint32_t caller_rva){
    if(!texture || !caller_rva)return true;
    // Cube/volume textures require distinct Lock signatures. Fail closed.
    if(texture->GetType()!=D3DRTYPE_TEXTURE){
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    std::lock_guard lock(g_texture_hook_mutex);
    if(g_texture_hook_unknown.load(std::memory_order_acquire))return false;
    auto** vt=*reinterpret_cast<void***>(texture);
    if(!vt || !vt[19] || !vt[20]){
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    if(g_texture_lock_target || g_texture_unlock_target){
        if(g_texture_lock_target==vt[19] &&
           g_texture_unlock_target==vt[20] &&
           g_texture_lock_rect && g_texture_unlock_rect)return true;
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    if(!Hook(vt[19],reinterpret_cast<void*>(&OriginalTextureLockRectHook),
             g_texture_lock_rect)){
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    if(!Hook(vt[20],reinterpret_cast<void*>(&OriginalTextureUnlockRectHook),
             g_texture_unlock_rect)){
        MH_DisableHook(vt[19]);MH_RemoveHook(vt[19]);
        g_texture_lock_rect=nullptr;
        OriginalTextureCoverageUnknown(caller_rva);return false;
    }
    g_texture_lock_target=vt[19];g_texture_unlock_target=vt[20];
    return true;
}
