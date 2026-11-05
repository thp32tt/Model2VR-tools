// Authentic primary EXE-owned D3D9 GPU-to-CPU readbacks. An unbound
// system-memory Surface can change without LockRect/GetDC or SetTexture.
// Preserve the real single COM call/HRESULT; never copy or log pixel data,
// Surface identities, handles, dimensions, or ROM content.
HRESULT STDMETHODCALLTYPE OriginalGetRenderTargetDataHook(
    IDirect3DDevice9* d,IDirect3DSurface9* source,
    IDirect3DSurface9* destination){
    if(!g_get_render_target_data)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_get_render_target_data(d,source,destination);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,68u,rva);
    else {
        g_original_surface_readbacks[0].fetch_add(1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::SurfaceTexelContent,rva,hr);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
        LogOriginalD3D9MutationPhase(6u,rva,hr);
        if(!destination || !ObserveOriginalTextureSurface(destination,rva))
            OriginalTextureCoverageUnknown(rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalGetFrontBufferDataHook(
    IDirect3DDevice9* d,UINT swap_chain,IDirect3DSurface9* destination){
    if(!g_get_front_buffer_data)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(d);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_get_front_buffer_data(d,swap_chain,destination);
    const auto rva=primary?CallerRva(caller):0u;
    if(!rva)return hr;
    if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,69u,rva);
    else {
        g_original_surface_readbacks[1].fetch_add(1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::SurfaceTexelContent,rva,hr);
        Model2VR_ObserveOriginalD3D9TextureUpload(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire));
        LogOriginalD3D9MutationPhase(7u,rva,hr);
        if(!destination || !ObserveOriginalTextureSurface(destination,rva))
            OriginalTextureCoverageUnknown(rva);
    }
    return hr;
}
