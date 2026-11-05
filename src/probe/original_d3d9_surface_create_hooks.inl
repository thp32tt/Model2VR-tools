// Authentic original x86 device Surface creation before the first use.
// Track source writes through the existing Surface LockRect/GetDC hooks,
// including offscreen staging sources not reachable from GetSurfaceLevel.
// Never copy/log Surface contents, dimensions, format, pointers, or DCs.
HRESULT STDMETHODCALLTYPE OriginalCreateRenderTargetHook(IDirect3DDevice9* d,
    UINT width,UINT height,D3DFORMAT format,D3DMULTISAMPLE_TYPE multi,
    DWORD quality,BOOL lockable,IDirect3DSurface9** out,HANDLE* shared){
    if(!g_create_render_target)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_render_target(
        d,width,height,format,multi,quality,lockable,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,65u,rva);
    else if(!out || !*out)OriginalTextureCoverageUnknown(rva);
    else {
        g_original_surface_creates[0].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalTextureSurface(*out,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCreateDepthStencilSurfaceHook(IDirect3DDevice9* d,
    UINT width,UINT height,D3DFORMAT format,D3DMULTISAMPLE_TYPE multi,
    DWORD quality,BOOL discard,IDirect3DSurface9** out,HANDLE* shared){
    if(!g_create_depth_stencil_surface)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_depth_stencil_surface(
        d,width,height,format,multi,quality,discard,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,66u,rva);
    else if(!out || !*out)OriginalTextureCoverageUnknown(rva);
    else {
        g_original_surface_creates[1].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalTextureSurface(*out,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCreateOffscreenPlainSurfaceHook(IDirect3DDevice9* d,
    UINT width,UINT height,D3DFORMAT format,D3DPOOL pool,
    IDirect3DSurface9** out,HANDLE* shared){
    if(!g_create_offscreen_plain_surface)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_offscreen_plain_surface(
        d,width,height,format,pool,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,67u,rva);
    else if(!out || !*out)OriginalTextureCoverageUnknown(rva);
    else {
        g_original_surface_creates[2].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalTextureSurface(*out,rva);
    }
    return hr;
}
