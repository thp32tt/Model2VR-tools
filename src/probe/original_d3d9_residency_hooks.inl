// Original Device9::EvictManagedResources COM 5, without raw resource data.
// Revoke speculative Win64 XR eyes BEFORE original COM changes residency.
void OriginalResidencySourceBoundary(std::uint32_t rva,
                                    std::uint32_t reason) noexcept {
    const auto frame=static_cast<std::uint64_t>(
        g_present_count.load(std::memory_order_acquire))+1u;
    if(g_residency_rejected_present.load(std::memory_order_acquire)>=frame)return;
    Model2VR_RejectOriginalD3D9ManagedResidency(
        frame,reason==1u?rva:0u,
        reason==1u?g_original_frame_draw_ordinal.load(
            std::memory_order_acquire):0u,reason);
    auto old=g_residency_rejected_present.load(std::memory_order_acquire);
    while(old<frame && !g_residency_rejected_present.compare_exchange_weak(
        old,frame,std::memory_order_acq_rel,std::memory_order_acquire)){}
}
HRESULT STDMETHODCALLTYPE OriginalEvictManagedResourcesHook(IDirect3DDevice9* device){
    if(!g_evict_managed_resources)return D3DERR_INVALIDCALL;
    if(m2vr::daytona::IsEyeReplayActive() || !g_primary_device.Owns(device))
        return g_evict_managed_resources(device);
    const auto rva=CallerRva(_ReturnAddress());
    if(rva)OriginalResidencySourceBoundary(rva,1u);
    else {
        g_residency_foreign_unproven.store(true,std::memory_order_release);
        OriginalResidencySourceBoundary(0u,2u);
    }
    const HRESULT hr=g_evict_managed_resources(device);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,75u,rva);
    if(rva && SUCCEEDED(hr))
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::DeviceEvictManagedResources,rva,hr);
    return hr;
}
