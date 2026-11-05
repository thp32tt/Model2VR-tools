// Original EXE-owned BaseTexture9 auto-mip and shape-specific dirty regions.
// No original pixel/region/filter data, resource identity or ROM capture.
unsigned MipFamily(D3DRESOURCETYPE shape)noexcept {
    return shape==D3DRTYPE_TEXTURE?0u:shape==D3DRTYPE_CUBETEXTURE?1u:
           shape==D3DRTYPE_VOLUMETEXTURE?2u:3u;
}
void RecordMipSourceWrite(m2vr::drawprobe::OriginalLineageKind kind,
                          std::uint32_t rva)noexcept {
    RecordOriginalLineageWrite(kind,rva,S_OK);
    if(g_original_texture_write_locks.HasOutstandingWrites())
        OriginalTextureCoverageUnknown(rva);
    Model2VR_ObserveOriginalD3D9TextureUpload(
        g_present_count.load(std::memory_order_acquire)+1u,rva,
        g_original_frame_draw_ordinal.load(std::memory_order_acquire));
}
DWORD STDMETHODCALLTYPE OriginalMipLodHook(IDirect3DBaseTexture9* t,DWORD lod){
    const unsigned f=t?MipFamily(t->GetType()):3u;
    if(f>=3u || !g_mip_lod[f])return 0u;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const DWORD previous=g_mip_lod[f](t,lod); // original return unchanged
    const auto rva=primary?CallerRva(caller):0u;
    // Managed texture LOD is a source sampling-state write; nonmanaged
    // resources may ignore it. Conservatively revoke incomplete eyes.
    if(rva)RecordMipSourceWrite(
        m2vr::drawprobe::OriginalLineageKind::TextureManagedLOD,rva);
    return previous;
}
HRESULT STDMETHODCALLTYPE OriginalMipFilterHook(IDirect3DBaseTexture9* t,
    D3DTEXTUREFILTERTYPE filter){
    const unsigned f=t?MipFamily(t->GetType()):3u;
    if(f>=3u || !g_mip_filter[f])return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_mip_filter[f](t,filter);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,56u,rva);
    if(rva && SUCCEEDED(hr))RecordMipSourceWrite(
        m2vr::drawprobe::OriginalLineageKind::TextureAutoGenFilter,rva);
    return hr;
}
void STDMETHODCALLTYPE OriginalMipGenerateHook(IDirect3DBaseTexture9* t){
    const unsigned f=t?MipFamily(t->GetType()):3u;
    if(f>=3u || !g_mip_generate[f])return;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const auto rva=primary?CallerRva(caller):0u;
    // No HRESULT (void COM): invalidate any staged eyes before GPU writes.
    if(rva)RecordMipSourceWrite(
        m2vr::drawprobe::OriginalLineageKind::TextureMipGeneration,rva);
    g_mip_generate[f](t); // exactly one original COM call
}
HRESULT STDMETHODCALLTYPE OriginalTextureDirtyHook(IDirect3DTexture9* t,
    const RECT* region){
    if(!g_texture_dirty)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_texture_dirty(t,region);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,57u,rva);
    if(rva && SUCCEEDED(hr))RecordMipSourceWrite(
        m2vr::drawprobe::OriginalLineageKind::TextureDirtyRegion,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCubeDirtyHook(IDirect3DCubeTexture9* t,
    D3DCUBEMAP_FACES face,const RECT* region){
    if(!g_cube_dirty)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_cube_dirty(t,face,region);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,58u,rva);
    if(rva && SUCCEEDED(hr))RecordMipSourceWrite(
        m2vr::drawprobe::OriginalLineageKind::TextureDirtyRegion,rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeDirtyHook(IDirect3DVolumeTexture9* t,
    const D3DBOX* region){
    if(!g_volume_dirty)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(t);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_dirty(t,region);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,59u,rva);
    if(rva && SUCCEEDED(hr))RecordMipSourceWrite(
        m2vr::drawprobe::OriginalLineageKind::TextureDirtyRegion,rva);
    return hr;
}
bool ObserveOriginalBoundMipTexture(IDirect3DBaseTexture9* t,
                                     std::uint32_t rva){
    if(!t || !rva)return false;
    const unsigned f=MipFamily(t->GetType());
    if(f>=3u){OriginalTextureCoverageUnknown(rva);return false;}
    std::lock_guard lock(g_mip_mutex);
    if(g_texture_hook_unknown.load(std::memory_order_acquire))return false;
    auto** vt=*reinterpret_cast<void***>(t);
    if(!vt || !vt[11] || !vt[14] || !vt[16] || !vt[21]){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(g_mip_lod_target[f] || g_mip_filter_target[f] ||
       g_mip_generate_target[f] || g_dirty_target[f]){
        if(g_mip_lod_target[f]==vt[11] && g_mip_lod[f] &&
           g_mip_filter_target[f]==vt[14] &&
           g_mip_generate_target[f]==vt[16] &&
           g_dirty_target[f]==vt[21] && g_mip_filter[f] && g_mip_generate[f] &&
           (f==0u?g_texture_dirty!=nullptr:
            f==1u?g_cube_dirty!=nullptr:g_volume_dirty!=nullptr))return true;
        OriginalTextureCoverageUnknown(rva);return false;
    }
    // Dirty slot 21 differs by texture shape and cannot share a detour.
    for(unsigned other=0;other<3u;++other)
        if(other!=f && g_dirty_target[other]==vt[21]){
            OriginalTextureCoverageUnknown(rva);return false;
        }
    unsigned shared_lod=3u,shared_filter=3u,shared_generate=3u;
    for(unsigned other=0;other<3u;++other){
        if(other==f)continue;
        if(g_mip_lod_target[other]==vt[11])shared_lod=other;
        if(g_mip_filter_target[other]==vt[14])shared_filter=other;
        if(g_mip_generate_target[other]==vt[16])shared_generate=other;
    }
    if(shared_lod<3u)g_mip_lod[f]=g_mip_lod[shared_lod];
    else if(!Hook(vt[11],reinterpret_cast<void*>(&OriginalMipLodHook),
                  g_mip_lod[f])){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(shared_filter<3u)g_mip_filter[f]=g_mip_filter[shared_filter];
    else if(!Hook(vt[14],reinterpret_cast<void*>(&OriginalMipFilterHook),
                  g_mip_filter[f])){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    if(shared_generate<3u)g_mip_generate[f]=g_mip_generate[shared_generate];
    else if(!Hook(vt[16],reinterpret_cast<void*>(&OriginalMipGenerateHook),
                  g_mip_generate[f])){
        OriginalTextureCoverageUnknown(rva);return false;
    }
    const bool dirty=f==0u?
        Hook(vt[21],reinterpret_cast<void*>(&OriginalTextureDirtyHook),
             g_texture_dirty):f==1u?
        Hook(vt[21],reinterpret_cast<void*>(&OriginalCubeDirtyHook),
             g_cube_dirty):
        Hook(vt[21],reinterpret_cast<void*>(&OriginalVolumeDirtyHook),
             g_volume_dirty);
    if(!dirty){
        // Any already-installed partial hooks still forward the real COM;
        // unknown source coverage permanently rejects speculative stereo.
        OriginalTextureCoverageUnknown(rva);return false;
    }
    g_mip_lod_target[f]=vt[11];
    g_mip_filter_target[f]=vt[14];
    g_mip_generate_target[f]=vt[16];
    g_dirty_target[f]=vt[21];
    return true;
}
