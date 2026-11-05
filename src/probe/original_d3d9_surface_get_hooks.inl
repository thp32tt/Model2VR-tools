// Genuine primary EXE-owned GetBackBuffer / GetRenderTarget /
// GetDepthStencilSurface COM aliases. These are read-only operations:
// an ordinary failure (e.g. no depth target) is not a failed mutation.
// A successful output is registered before later CPU/GDI Surface writes.
// No Surface pointer, dimensions, backbuffer pixels or original game data
// enter a diagnostic event or the Win64 IPC channel.
HRESULT STDMETHODCALLTYPE OriginalGetBackBufferHook(IDirect3DDevice9* d,
    UINT swap_chain,UINT back_buffer,D3DBACKBUFFER_TYPE type,
    IDirect3DSurface9** out){
    if(!g_get_back_buffer)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_get_back_buffer(d,swap_chain,back_buffer,type,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr)){
        g_original_surface_getters[0].fetch_add(1u,std::memory_order_relaxed);
        if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
            OriginalTextureCoverageUnknown(rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalGetRenderTargetHook(IDirect3DDevice9* d,
    DWORD index,IDirect3DSurface9** out){
    if(!g_get_render_target)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_get_render_target(d,index,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr)){
        g_original_surface_getters[1].fetch_add(1u,std::memory_order_relaxed);
        if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
            OriginalTextureCoverageUnknown(rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalGetDepthStencilSurfaceHook(
    IDirect3DDevice9* d,IDirect3DSurface9** out){
    if(!g_get_depth_stencil_surface)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_get_depth_stencil_surface(d,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr)){
        g_original_surface_getters[2].fetch_add(1u,std::memory_order_relaxed);
        if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
            OriginalTextureCoverageUnknown(rva);
    }
    return hr;
}
