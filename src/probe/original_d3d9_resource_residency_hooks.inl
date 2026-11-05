// Genuine original D3D9Resource9::SetPriority(7)/PreLoad(9) COM interception.
// These resource-level calls are real and can affect residency or when
// managed VB/IB, 2D/cube/volume textures and surfaces upload to VRAM.
// A successful source Draw/Present without this residency lineage is not
// an authenticated OpenXR eye source. Never inspect pixel/vertex bytes.
// Every original COM call happens EXACTLY ONCE with its original result.
using OriginalSetResourcePriorityFn=DWORD (STDMETHODCALLTYPE*)(
    IDirect3DResource9*,DWORD);
using OriginalResourcePreLoadFn=void (STDMETHODCALLTYPE*)(
    IDirect3DResource9*);
void OriginalResidencySourceBoundary(std::uint32_t,std::uint32_t) noexcept;
template<class T> struct OriginalResourceHookTarget {
    void* address=nullptr;
    T original=nullptr;
};
// D3D9 can share a COM implementation between texture types, or use a
// different implementation for each VB/IB/Surface/Texture. Never assume
// one single trampoline works for all resource implementations. Bounded.
inline constexpr std::size_t kOriginalResourceHookSlots=12u;
std::mutex g_original_resource_residency_mutex;
std::array<OriginalResourceHookTarget<OriginalSetResourcePriorityFn>,
    kOriginalResourceHookSlots> g_original_priority_targets{};
std::array<OriginalResourceHookTarget<OriginalResourcePreLoadFn>,
    kOriginalResourceHookSlots> g_original_preload_targets{};
std::array<std::atomic<std::uint32_t>,2> g_original_resource_residency_calls{};
void OriginalResourceResidencyUnknown(std::uint32_t rva) noexcept {
    // Hook coverage is never inferred from successful texture bindings.
    // Persist through Reset and reject every later physical Present.
    g_residency_foreign_unproven.store(true,std::memory_order_release);
    OriginalResidencySourceBoundary(0u,3u);
    if(rva)m2vr::LogEvent("model2_d3d9_original_resource_residency_unobserved",
        g_present_count.load(std::memory_order_relaxed)+1u,rva,0u,0u);
}
template<class Fn>
Fn OriginalResourceTrampoline(
    const std::array<OriginalResourceHookTarget<Fn>,
       kOriginalResourceHookSlots>& targets,void* vtable_target) noexcept {
    std::lock_guard lock(g_original_resource_residency_mutex);
    for(const auto& slot:targets)
        if(slot.address==vtable_target && slot.original)
            return slot.original;
    return nullptr;
}
void OriginalResourceMutationBeforeCom(std::uint32_t caller_rva) noexcept {
    if(caller_rva)OriginalResidencySourceBoundary(caller_rva,1u);
    else {
        g_residency_foreign_unproven.store(true,std::memory_order_release);
        OriginalResidencySourceBoundary(0u,2u);
    }
}
DWORD STDMETHODCALLTYPE OriginalResourceSetPriorityHook(
    IDirect3DResource9* resource,DWORD priority){
    auto** vt=resource?*reinterpret_cast<void***>(resource):nullptr;
    const auto original=OriginalResourceTrampoline(
        g_original_priority_targets,vt?vt[7]:nullptr);
    if(!original)return 0u; // unreachable for an installed complete hook
    void* const origin=_ReturnAddress();
    const bool primary=PrimaryOriginalResource(resource);
    const auto rva=primary?CallerRva(origin):0u;
    if(primary)OriginalResourceMutationBeforeCom(rva);
    const DWORD previous=original(resource,priority);
    if(rva){
        g_original_resource_residency_calls[0].fetch_add(1u,
            std::memory_order_relaxed);
        // D3D9 SetPriority returns the previous numeric priority, NOT
        // HRESULT. Never classify its high bit as a failure.
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::ResourceSetPriority,rva,S_OK);
    }
    return previous;
}
void STDMETHODCALLTYPE OriginalResourcePreLoadHook(IDirect3DResource9* resource){
    auto** vt=resource?*reinterpret_cast<void***>(resource):nullptr;
    const auto original=OriginalResourceTrampoline(
        g_original_preload_targets,vt?vt[9]:nullptr);
    if(!original)return; // unreachable for an installed complete hook
    void* const origin=_ReturnAddress();
    const bool primary=PrimaryOriginalResource(resource);
    const auto rva=primary?CallerRva(origin):0u;
    if(primary)OriginalResourceMutationBeforeCom(rva);
    original(resource);
    if(rva){
        g_original_resource_residency_calls[1].fetch_add(1u,
            std::memory_order_relaxed);
        // PreLoad is void: absence of HRESULT does not authenticate a
        // gameplay world pass, and the original operation is not replayed.
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::ResourcePreLoad,rva,S_OK);
    }
}
template<class Fn>
bool EnsureResourceHookTarget(
    std::array<OriginalResourceHookTarget<Fn>,
        kOriginalResourceHookSlots>& slots,void* target,void* detour,
    bool& installed_new) {
    installed_new=false;
    for(const auto& item:slots)
        if(item.address==target)return item.original!=nullptr;
    for(auto& item:slots){
        if(item.address)continue;
        // The registry mutex stays held through MH_EnableHook and the
        // target publication. Any incoming detour waits for this slot.
        if(!Hook(target,detour,item.original))return false;
        item.address=target;
        installed_new=true;
        return true;
    }
    return false;
}
template<class Fn>
void RollbackOriginalResourceHook(
    std::array<OriginalResourceHookTarget<Fn>,
        kOriginalResourceHookSlots>& slots,void* target) {
    for(auto& item:slots){
        if(item.address!=target)continue;
        MH_DisableHook(target);MH_RemoveHook(target);
        item={};
        return;
    }
}
bool EnsureOriginalResourceResidencyObserved(
    IDirect3DResource9* resource,std::uint32_t caller_rva) {
    if(!resource || !caller_rva)return false;
    auto** vt=*reinterpret_cast<void***>(resource);
    if(!vt || !vt[7] || !vt[9] || vt[7]==vt[9]){
        OriginalResourceResidencyUnknown(caller_rva);
        return false;
    }
    bool priority_new=false,preload_new=false;
    bool complete=false;
    {
        std::lock_guard lock(g_original_resource_residency_mutex);
        const bool priority=EnsureResourceHookTarget(
            g_original_priority_targets,vt[7],
            reinterpret_cast<void*>(&OriginalResourceSetPriorityHook),
            priority_new);
        const bool preload=priority && EnsureResourceHookTarget(
            g_original_preload_targets,vt[9],
            reinterpret_cast<void*>(&OriginalResourcePreLoadHook),
            preload_new);
        complete=priority && preload;
        if(!complete && priority_new)
            RollbackOriginalResourceHook(g_original_priority_targets,vt[7]);
    }
    if(!complete)OriginalResourceResidencyUnknown(caller_rva);
    return complete;
}
