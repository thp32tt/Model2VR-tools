// Genuine original x86 primary device VB/IB Create calls (slots 26/27).
// Observe returned resource Lock/Unlock before first SetStreamSource/SetIndices,
// retaining exact original HRESULT, resource pointer, memory and game timeline.
HRESULT STDMETHODCALLTYPE OriginalCreateVertexBufferHook(IDirect3DDevice9* d,
    UINT length,DWORD usage,DWORD fvf,D3DPOOL pool,
    IDirect3DVertexBuffer9** out,HANDLE* shared){
    if(!g_create_vertex_buffer)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_vertex_buffer(d,length,usage,fvf,pool,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,63u,rva);
    else if(!out || !*out)OriginalBufferCoverageUnknown(rva,1u);
    else {
        g_original_buffer_creates[0].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalBoundBuffer(*out,1u,rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalCreateIndexBufferHook(IDirect3DDevice9* d,
    UINT length,DWORD usage,D3DFORMAT format,D3DPOOL pool,
    IDirect3DIndexBuffer9** out,HANDLE* shared){
    if(!g_create_index_buffer)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_create_index_buffer(d,length,usage,format,pool,out,shared);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,64u,rva);
    else if(!out || !*out)OriginalBufferCoverageUnknown(rva,2u);
    else {
        g_original_buffer_creates[1].fetch_add(1u,std::memory_order_relaxed);
        ObserveOriginalBoundBuffer(*out,2u,rva);
    }
    return hr;
}
