// Original EXE-owned Resource9::SetPrivateData(4)/FreePrivateData(6).
// These COM metadata operations can change original resource identity/alias
// references after a witnessed Draw. They are NOT World3D proof. Preserve
// original GUID, bytes, flags, COM result and original execution exactly once.
// Never log GUIDs, pointer values or the contents of the caller's data.
using OriginalResourceSetPrivateDataFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DResource9*,REFGUID,const void*,DWORD,DWORD);
using OriginalResourceFreePrivateDataFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DResource9*,REFGUID);
std::array<OriginalResourceHookTarget<OriginalResourceSetPrivateDataFn>,
    kOriginalResourceHookSlots> g_original_private_set_targets{};
std::array<OriginalResourceHookTarget<OriginalResourceFreePrivateDataFn>,
    kOriginalResourceHookSlots> g_original_private_free_targets{};
std::array<std::atomic<std::uint32_t>,2> g_original_private_data_calls{};
std::array<std::atomic<std::uint64_t>,2> g_original_private_data_logged_frame{};
void RecordOriginalResourcePrivateDataPhase(std::uint32_t rva,
                                            std::uint32_t method) noexcept {
    if(!rva || method<1u || method>2u)return;
    const auto frame=std::uint64_t{
        g_present_count.load(std::memory_order_acquire)}+1u;
    const auto ordinal=g_original_frame_draw_ordinal.load(
        std::memory_order_acquire);
    if((frame!=1u && frame%8u!=0u) || ordinal>10000000u)return;
    auto& claimed=g_original_private_data_logged_frame[method-1u];
    auto prior=claimed.load(std::memory_order_acquire);
    while(prior<frame && !claimed.compare_exchange_weak(
        prior,frame,std::memory_order_acq_rel,std::memory_order_acquire)){}
    if(prior>=frame)return;
    m2vr::LogEvent("model2_d3d9_original_resource_private_data_phase",
        frame,rva,ordinal,method);
}
HRESULT STDMETHODCALLTYPE OriginalResourceSetPrivateDataHook(
    IDirect3DResource9* resource,REFGUID key,const void* data,
    DWORD bytes,DWORD flags) {
    auto** vt=resource?*reinterpret_cast<void***>(resource):nullptr;
    const auto original=OriginalResourceTrampoline(
        g_original_private_set_targets,vt?vt[4]:nullptr);
    if(!original)return D3DERR_INVALIDCALL; // no complete installed hook
    if(m2vr::daytona::IsEyeReplayActive() ||
       g_authenticated_original_resource_depth)
        return original(resource,key,data,bytes,flags);
    void* const origin=_ReturnAddress();
    const auto owner=OriginalResourceDeviceOwner(resource);
    const bool primary=owner==m2vr::vrbridge::OriginalResourceOwner::Primary;
    const auto rva=primary?CallerRva(origin):0u;
    if(primary)OriginalResourceMutationBeforeCom(rva);
    else if(owner==m2vr::vrbridge::OriginalResourceOwner::Unknown)
        OriginalResourceResidencyUnknown(CallerRva(origin));
    ScopedAuthenticatedOriginalResourceCall scope(primary && rva);
    // D3DSPD_IUNKNOWN ownership/ref-count behaviour belongs to the original
    // D3D9 runtime. Never inspect or copy the input payload, even on failure.
    const HRESULT hr=original(resource,key,data,bytes,flags);
    if(rva){
        g_original_private_data_calls[0].fetch_add(
            1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::ResourceSetPrivateData,
            rva,hr);
        RecordOriginalResourcePrivateDataPhase(rva,1u);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalResourceFreePrivateDataHook(
    IDirect3DResource9* resource,REFGUID key) {
    auto** vt=resource?*reinterpret_cast<void***>(resource):nullptr;
    const auto original=OriginalResourceTrampoline(
        g_original_private_free_targets,vt?vt[6]:nullptr);
    if(!original)return D3DERR_INVALIDCALL; // no complete installed hook
    if(m2vr::daytona::IsEyeReplayActive() ||
       g_authenticated_original_resource_depth)
        return original(resource,key);
    void* const origin=_ReturnAddress();
    const auto owner=OriginalResourceDeviceOwner(resource);
    const bool primary=owner==m2vr::vrbridge::OriginalResourceOwner::Primary;
    const auto rva=primary?CallerRva(origin):0u;
    if(primary)OriginalResourceMutationBeforeCom(rva);
    else if(owner==m2vr::vrbridge::OriginalResourceOwner::Unknown)
        OriginalResourceResidencyUnknown(CallerRva(origin));
    ScopedAuthenticatedOriginalResourceCall scope(primary && rva);
    const HRESULT hr=original(resource,key);
    if(rva){
        g_original_private_data_calls[1].fetch_add(
            1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::ResourceFreePrivateData,
            rva,hr);
        RecordOriginalResourcePrivateDataPhase(rva,2u);
    }
    return hr;
}
bool EnsureOriginalResourcePrivateDataObserved(
    IDirect3DResource9* resource,std::uint32_t caller_rva) {
    if(!resource || !caller_rva)return false;
    auto** vt=*reinterpret_cast<void***>(resource);
    if(!vt || !vt[4] || !vt[6] || vt[4]==vt[6])return false;
    std::lock_guard lock(g_original_resource_residency_mutex);
    const bool set=EnsureResourceHookTarget(g_original_private_set_targets,
        vt[4],reinterpret_cast<void*>(&OriginalResourceSetPrivateDataHook));
    const bool free=set && EnsureResourceHookTarget(g_original_private_free_targets,
        vt[6],reinterpret_cast<void*>(&OriginalResourceFreePrivateDataHook));
    // On partial installation never MH_DisableHook or free its trampoline.
    // Retained hooks remain COM passthrough; original stereo becomes unproven.
    return set && free;
}
