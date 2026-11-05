// Genuine primary EXE-owned binding getters. These COM methods are
// read-only: returning no bound resource is legal and FAILED(hr) is not a
// source-content mutation. Install existing CPU-mapping listeners on an
// authentic non-null returned resource BEFORE its next Lock/Unlock.
// No stage, offset, stride, resource pointer, pixel or vertex bytes logged.
HRESULT STDMETHODCALLTYPE OriginalGetTextureHook(IDirect3DDevice9* d,
    DWORD stage,IDirect3DBaseTexture9** out){
    if(!g_get_texture)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_get_texture(d,stage,out);
    const auto rva=primary?CallerRva(origin):0u;
    if(rva && SUCCEEDED(hr)){
        g_original_resource_getters[0].fetch_add(1u,std::memory_order_relaxed);
        if(!out)OriginalTextureCoverageUnknown(rva);
        else if(*out)ObserveOriginalBoundTexture(*out,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalGetStreamSourceHook(IDirect3DDevice9* d,
    UINT stream,IDirect3DVertexBuffer9** out,UINT* offset,UINT* stride){
    if(!g_get_stream_source)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_get_stream_source(d,stream,out,offset,stride);
    const auto rva=primary?CallerRva(origin):0u;
    if(rva && SUCCEEDED(hr)){
        g_original_resource_getters[1].fetch_add(1u,std::memory_order_relaxed);
        if(!out)OriginalBufferCoverageUnknown(rva,1u);
        else if(*out)ObserveOriginalBoundBuffer(*out,1u,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalGetIndicesHook(IDirect3DDevice9* d,
    IDirect3DIndexBuffer9** out){
    if(!g_get_indices)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_get_indices(d,out);
    const auto rva=primary?CallerRva(origin):0u;
    if(rva && SUCCEEDED(hr)){
        g_original_resource_getters[2].fetch_add(1u,std::memory_order_relaxed);
        if(!out)OriginalBufferCoverageUnknown(rva,2u);
        else if(*out)ObserveOriginalBoundBuffer(*out,2u,rva);
    }
    return hr;
}
