// Original EXE-owned D3D9 texture creation (device slots 23/24/25).
// Installs existing COM content/shape/mip observers IMMEDIATELY, before the
// game's first SetTexture binding. Keep original result and resource lifetime.
HRESULT STDMETHODCALLTYPE OriginalCreateTextureHook(IDirect3DDevice9* d,
    UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,
    IDirect3DTexture9** out,HANDLE* shared) {
    if(!g_create_texture)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_texture(d,width,height,levels,usage,format,pool,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,60u,rva);
    else if(!out || !*out)OriginalTextureCoverageUnknown(rva);
    else {
        g_original_texture_creates[0].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalBoundTexture(*out,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCreateVolumeTextureHook(IDirect3DDevice9* d,
    UINT width,UINT height,UINT depth,UINT levels,DWORD usage,D3DFORMAT format,
    D3DPOOL pool,IDirect3DVolumeTexture9** out,HANDLE* shared) {
    if(!g_create_volume_texture)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_volume_texture(
        d,width,height,depth,levels,usage,format,pool,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,61u,rva);
    else if(!out || !*out)OriginalTextureCoverageUnknown(rva);
    else {
        g_original_texture_creates[2].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalBoundTexture(*out,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCreateCubeTextureHook(IDirect3DDevice9* d,
    UINT edge,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,
    IDirect3DCubeTexture9** out,HANDLE* shared) {
    if(!g_create_cube_texture)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_cube_texture(d,edge,levels,usage,format,pool,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,62u,rva);
    else if(!out || !*out)OriginalTextureCoverageUnknown(rva);
    else {
        g_original_texture_creates[1].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalBoundTexture(*out,rva);
    }
    return hr;
}
