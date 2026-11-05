// Authentic IDirect3DDevice9::CreateQuery(118) and Query9::Issue/GetData(6/7).
// Preserve original EXE COM, HRESULT and GetData S_FALSE pending semantics.
// Query answers can influence CPU occlusion culling without a new Draw state
// write. Never read or copy result bytes into the Win64/OpenXR path.
enum class OriginalQueryOwner : std::uint32_t { Unknown=0u,Primary=1u,Secondary=2u };
OriginalQueryOwner OriginalQueryDeviceOwner(IDirect3DQuery9* query) noexcept {
    if(!query)return OriginalQueryOwner::Unknown;
    IDirect3DDevice9* device=nullptr;
    const HRESULT hr=query->GetDevice(&device);
    const bool known=SUCCEEDED(hr) && device;
    const bool primary=known && g_primary_device.Owns(device);
    if(device)device->Release();
    return !known?OriginalQueryOwner::Unknown:
        (primary?OriginalQueryOwner::Primary:OriginalQueryOwner::Secondary);
}
// Coalesce repeated GetData polling into one real physical-Present rejection.
void OriginalQuerySourceBoundary(std::uint32_t rva,std::uint32_t reason) noexcept {
    const auto frame=static_cast<std::uint64_t>(
        g_present_count.load(std::memory_order_acquire))+1u;
    auto old=g_original_query_rejected_present.load(std::memory_order_acquire);
    while(old<frame){
        if(g_original_query_rejected_present.compare_exchange_weak(
                old,frame,std::memory_order_acq_rel,std::memory_order_acquire)){
            Model2VR_RejectOriginalD3D9QueryUse(
                frame,reason==3u?0u:rva,
                reason==3u?0u:g_original_frame_draw_ordinal.load(
                    std::memory_order_acquire),reason);
            return;
        }
    }
}
void OriginalQueryCoverageUnknown() noexcept {
    g_original_query_source_unproven.store(true,std::memory_order_release);
    OriginalQuerySourceBoundary(0u,3u);
}
HRESULT STDMETHODCALLTYPE OriginalQueryIssueHook(IDirect3DQuery9* query,DWORD flags){
    if(!g_original_query_issue)return D3DERR_INVALIDCALL;
    const auto rva=CallerRva(_ReturnAddress());
    const auto owner=OriginalQueryDeviceOwner(query);
    if(owner==OriginalQueryOwner::Primary){
        if(rva)OriginalQuerySourceBoundary(rva,1u);
        else OriginalQueryCoverageUnknown();
    }else if(owner==OriginalQueryOwner::Unknown)OriginalQueryCoverageUnknown();
    const HRESULT hr=g_original_query_issue(query,flags);
    if(owner==OriginalQueryOwner::Primary && rva){
        if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,73u,rva);
        else {
            g_original_query_calls[1].fetch_add(1u,std::memory_order_relaxed);
            RecordOriginalLineageWrite(
                m2vr::drawprobe::OriginalLineageKind::QueryIssue,rva,hr);
        }
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalQueryGetDataHook(
    IDirect3DQuery9* query,void* data,DWORD size,DWORD flags){
    if(!g_original_query_get_data)return D3DERR_INVALIDCALL;
    const auto rva=CallerRva(_ReturnAddress());
    const auto owner=OriginalQueryDeviceOwner(query);
    if(owner==OriginalQueryOwner::Primary){
        if(rva)OriginalQuerySourceBoundary(rva,2u);
        else OriginalQueryCoverageUnknown();
    }else if(owner==OriginalQueryOwner::Unknown)OriginalQueryCoverageUnknown();
    const HRESULT hr=g_original_query_get_data(query,data,size,flags);
    if(owner==OriginalQueryOwner::Primary && rva){
        g_original_query_calls[2].fetch_add(1u,std::memory_order_relaxed);
        if(FAILED(hr))TrackFailedOriginalD3D9Surface(hr,74u,rva);
        else if(hr==S_OK){
            g_original_query_calls[3].fetch_add(1u,std::memory_order_relaxed);
            RecordOriginalLineageWrite(
                m2vr::drawprobe::OriginalLineageKind::QueryReady,rva,hr);
        }
    }
    return hr;
}
bool EnsureOriginalQueryObserved(IDirect3DQuery9* query){
    if(!query)return false;
    auto** table=*reinterpret_cast<void***>(query);
    if(!table || !table[6] || !table[7])return false;
    std::lock_guard lock(g_original_query_hook_mutex);
    if(g_original_query_issue_target || g_original_query_get_data_target)
        return g_original_query_issue_target==table[6] &&
               g_original_query_get_data_target==table[7] &&
               g_original_query_issue && g_original_query_get_data;
    const bool issue=Hook(table[6],
        reinterpret_cast<void*>(&OriginalQueryIssueHook),g_original_query_issue);
    const bool data=Hook(table[7],
        reinterpret_cast<void*>(&OriginalQueryGetDataHook),g_original_query_get_data);
    if(!(issue&&data)){
        if(issue){MH_DisableHook(table[6]);MH_RemoveHook(table[6]);}
        if(data){MH_DisableHook(table[7]);MH_RemoveHook(table[7]);}
        g_original_query_issue=nullptr;g_original_query_get_data=nullptr;
        return false;
    }
    g_original_query_issue_target=table[6];
    g_original_query_get_data_target=table[7];
    return true;
}
HRESULT STDMETHODCALLTYPE OriginalCreateQueryHook(
    IDirect3DDevice9* device,D3DQUERYTYPE type,IDirect3DQuery9** out){
    if(!g_create_query)return D3DERR_INVALIDCALL;
    const bool primary=g_primary_device.Owns(device);
    const auto rva=primary?CallerRva(_ReturnAddress()):0u;
    const HRESULT hr=g_create_query(device,type,out);
    if(!primary || !out)return hr; // NULL is an official support query.
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,72u,rva);
    if(SUCCEEDED(hr)){
        if(!rva || !*out || !EnsureOriginalQueryObserved(*out))
            OriginalQueryCoverageUnknown();
        else g_original_query_calls[0].fetch_add(1u,std::memory_order_relaxed);
    }
    return hr;
}
