// Included only inside d3d9_probe.cpp's anonymous Win32 x86 namespace.
// Original IDirect3DVertexBuffer9 and IDirect3DIndexBuffer9 COM slots 11/12
// are dynamically discovered from real EXE-owned SetStreamSource/SetIndices.
// NEVER copy, patch, replay or log original vertices, indices or pointers.
bool PrimaryOriginalResource(IDirect3DResource9* resource) noexcept {
    if(!resource)return false;
    IDirect3DDevice9* device=nullptr;
    const HRESULT hr=resource->GetDevice(&device);
    const bool primary=SUCCEEDED(hr)&&device&&g_primary_device.Owns(device);
    if(device)device->Release();
    return primary;
}
BufferLockFn BufferLockTrampoline(D3DRESOURCETYPE kind) noexcept {
    return kind==D3DRTYPE_VERTEXBUFFER?g_vb_lock:
           kind==D3DRTYPE_INDEXBUFFER?g_ib_lock:nullptr;
}
BufferUnlockFn BufferUnlockTrampoline(D3DRESOURCETYPE kind) noexcept {
    return kind==D3DRTYPE_VERTEXBUFFER?g_vb_unlock:
           kind==D3DRTYPE_INDEXBUFFER?g_ib_unlock:nullptr;
}
void OriginalBufferCoverageUnknown(std::uint32_t caller_rva,unsigned family) noexcept;
HRESULT STDMETHODCALLTYPE OriginalBufferLockHook(IDirect3DResource9* resource,
    UINT offset,UINT length,void** destination,DWORD flags) {
    const auto kind=resource?resource->GetType():D3DRTYPE_FORCE_DWORD;
    const auto original=BufferLockTrampoline(kind);
    if(!original)return D3DERR_INVALIDCALL; // trampoline unavailable
    const bool primary=PrimaryOriginalResource(resource);
    void* const caller=_ReturnAddress();
    const HRESULT hr=original(resource,offset,length,destination,flags);
    if(!primary)return hr;
    const auto rva=CallerRva(caller);
    if(!rva)return hr; // overlays, independent eye-copy internals
    const unsigned family=kind==D3DRTYPE_VERTEXBUFFER?1u:2u;
    if(FAILED(hr)) {
        TrackFailedOriginalD3D9Surface(hr,family==1u?37u:38u,rva);
    } else if(m2vr::drawprobe::IsWritableBufferLock(flags)) {
        RecordOriginalLineageWrite(family==1u?
            m2vr::drawprobe::OriginalLineageKind::VertexBufferContent:
            m2vr::drawprobe::OriginalLineageKind::IndexBufferContent,rva,hr);
        g_original_buffer_writes[family-1u].fetch_add(
            1u,std::memory_order_relaxed);
        if(!g_original_bound_write_locks.Begin(resource))
            OriginalBufferCoverageUnknown(rva,family);
        // Successful writable Lock can be modified before Unlock. Invalidating
        // now guarantees a stale eye is not consumed on later game Present.
        Model2VR_ObserveOriginalD3D9BufferMutation(
            g_present_count.load(std::memory_order_acquire)+1u,rva,
            g_original_frame_draw_ordinal.load(std::memory_order_acquire),
            family);
    }
    return hr; // genuine original Lock HRESULT, untouched
}
HRESULT STDMETHODCALLTYPE OriginalBufferUnlockHook(IDirect3DResource9* resource){
    const auto kind=resource?resource->GetType():D3DRTYPE_FORCE_DWORD;
    const auto original=BufferUnlockTrampoline(kind);
    if(!original)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(resource);
    void* const caller=_ReturnAddress();
    const HRESULT hr=original(resource);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr))
        g_original_bound_write_locks.SuccessfulUnlock(resource);
    if(rva && FAILED(hr)) {
        TrackFailedOriginalD3D9Surface(
            hr,kind==D3DRTYPE_VERTEXBUFFER?39u:40u,rva);
    }
    return hr; // genuine original Unlock HRESULT, untouched
}
void OriginalBufferCoverageUnknown(std::uint32_t caller_rva,unsigned family) noexcept {
    // Persist across Reset: unknown resource implementations can appear again
    // without an observed Set*. Do not pretend source geometry was mirrored.
    g_buffer_hook_unknown.store(true,std::memory_order_release);
    Model2VR_RejectOriginalD3D9BufferHookCoverage(
        g_present_count.load(std::memory_order_acquire)+1u,caller_rva,family);
}
bool ObserveOriginalBoundBuffer(IDirect3DResource9* resource,
    unsigned family,std::uint32_t caller_rva) {
    if(!resource || !caller_rva || family<1u || family>2u)return true;
    std::lock_guard lock(g_buffer_hook_mutex);
    if(g_buffer_hook_unknown.load(std::memory_order_acquire))return false;
    auto** vt=*reinterpret_cast<void***>(resource);
    if(!vt || !vt[11] || !vt[12]) {
        OriginalBufferCoverageUnknown(caller_rva,family);
        return false;
    }
    auto& lock_target=family==1u?g_vb_lock_target:g_ib_lock_target;
    auto& unlock_target=family==1u?g_vb_unlock_target:g_ib_unlock_target;
    auto& lock_original=family==1u?g_vb_lock:g_ib_lock;
    auto& unlock_original=family==1u?g_vb_unlock:g_ib_unlock;
    if(lock_target || unlock_target) {
        if(lock_target==vt[11] && unlock_target==vt[12] &&
           lock_original && unlock_original)return true;
        OriginalBufferCoverageUnknown(caller_rva,family);return false;
    }
    const void* other_lock=family==1u?g_ib_lock_target:g_vb_lock_target;
    const void* other_unlock=family==1u?g_ib_unlock_target:g_vb_unlock_target;
    const bool shared_lock=vt[11]==other_lock;
    const bool shared_unlock=vt[12]==other_unlock;
    // Some D3D9 runtimes may share the same implementation across VB/IB.
    // Reuse the first trampoline if MinHook has already intercepted it.
    if(shared_lock)lock_original=family==1u?g_ib_lock:g_vb_lock;
    else if(!Hook(vt[11],reinterpret_cast<void*>(&OriginalBufferLockHook),
                  lock_original)) {
        OriginalBufferCoverageUnknown(caller_rva,family);return false;
    }
    if(shared_unlock)unlock_original=family==1u?g_ib_unlock:g_vb_unlock;
    else if(!Hook(vt[12],reinterpret_cast<void*>(&OriginalBufferUnlockHook),
                  unlock_original)) {
        if(!shared_lock){
            MH_DisableHook(vt[11]);MH_RemoveHook(vt[11]);lock_original=nullptr;
        }
        OriginalBufferCoverageUnknown(caller_rva,family);return false;
    }
    lock_target=vt[11];unlock_target=vt[12];
    return true;
}
