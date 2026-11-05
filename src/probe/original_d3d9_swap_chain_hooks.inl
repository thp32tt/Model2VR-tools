// Genuine primary original x86 D3D9 additional SwapChain boundary.
// Device CreateAdditionalSwapChain(13)/GetSwapChain(14) return an object
// whose Present(3) has its OWN physical presentation boundary. Never
// confuse it with primary IDirect3DDevice9::Present(17) or mint a World3D
// source from a read-only GetBackBuffer(5). All original COM calls run once.
// A surface returned here receives the existing CPU/GDI write observer before
// its first LockRect/GetDC, with no pixels or COM identities logged.
enum class OriginalSwapChainOwner : std::uint32_t {
    Unknown=0u, Primary=1u, Secondary=2u
};
OriginalSwapChainOwner OriginalSwapChainDeviceOwner(
    IDirect3DSwapChain9* chain) noexcept {
    if(!chain)return OriginalSwapChainOwner::Unknown;
    IDirect3DDevice9* device=nullptr;
    const HRESULT hr=chain->GetDevice(&device);
    const bool known=SUCCEEDED(hr) && device;
    const bool primary=known && g_primary_device.Owns(device);
    if(device)device->Release();
    if(!known)return OriginalSwapChainOwner::Unknown;
    return primary?OriginalSwapChainOwner::Primary:
                   OriginalSwapChainOwner::Secondary;
}
void OriginalSwapChainCoverageUnknown(std::uint32_t rva) noexcept {
    g_original_swap_chain_source_unproven.store(true,std::memory_order_release);
    Model2VR_RejectOriginalD3D9SwapChainPresent(
        g_present_count.load(std::memory_order_acquire)+1u,rva,2u);
}
HRESULT STDMETHODCALLTYPE OriginalSwapChainPresentHook(
    IDirect3DSwapChain9* chain,const RECT* src,const RECT* dst,
    HWND window,const RGNDATA* dirty,DWORD flags){
    if(!g_original_swap_chain_present)return D3DERR_INVALIDCALL;
    void* const origin=_ReturnAddress();
    const auto rva=CallerRva(origin);
    if(rva){
        const auto owner=OriginalSwapChainDeviceOwner(chain);
        if(owner==OriginalSwapChainOwner::Primary){
            g_original_swap_chain_calls[2].fetch_add(1u,std::memory_order_relaxed);
            // Revoke BOTH sources BEFORE genuine COM presentation. Preserve
            // reason=1 rather than overwriting it with generic uncertainty.
            g_original_swap_chain_source_unproven.store(
                true,std::memory_order_release);
            Model2VR_RejectOriginalD3D9SwapChainPresent(
                g_present_count.load(std::memory_order_acquire)+1u,rva,1u);
        }else if(owner==OriginalSwapChainOwner::Unknown){
            // A failed GetDevice cannot authenticate primary source ownership.
            // A known secondary device is NOT the game's primary Draw owner.
            OriginalSwapChainCoverageUnknown(rva);
        }
    }
    return g_original_swap_chain_present(chain,src,dst,window,dirty,flags);
}
// SwapChain9::GetFrontBufferData(4) writes genuine GPU content to a
// CPU-visible Surface without going through Device9::GetFrontBufferData(33).
HRESULT STDMETHODCALLTYPE OriginalSwapChainGetFrontBufferDataHook(
    IDirect3DSwapChain9* chain,IDirect3DSurface9* destination){
    if(!g_original_swap_chain_get_front_buffer_data)return D3DERR_INVALIDCALL;
    void* const origin=_ReturnAddress();
    const auto rva=CallerRva(origin);
    const auto owner=rva?OriginalSwapChainDeviceOwner(chain):
                         OriginalSwapChainOwner::Secondary;
    if(rva && owner==OriginalSwapChainOwner::Primary){
        g_original_swap_chain_source_unproven.store(
            true,std::memory_order_release);
        // Pre-revoke CPU-world and GPU eyes before the actual GPU copy.
        Model2VR_RejectOriginalD3D9SwapChainPresent(
            g_present_count.load(std::memory_order_acquire)+1u,rva,3u);
    }else if(rva && owner==OriginalSwapChainOwner::Unknown){
        OriginalSwapChainCoverageUnknown(rva);
    }
    const HRESULT hr=g_original_swap_chain_get_front_buffer_data(
        chain,destination);
    if(rva && owner==OriginalSwapChainOwner::Primary && SUCCEEDED(hr)){
        g_original_swap_chain_readbacks.fetch_add(1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::SurfaceTexelContent,rva,hr);
        if(!destination || !ObserveOriginalTextureSurface(destination,rva))
            OriginalSwapChainCoverageUnknown(rva);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalSwapChainGetBackBufferHook(
    IDirect3DSwapChain9* chain,UINT index,D3DBACKBUFFER_TYPE type,
    IDirect3DSurface9** out){
    if(!g_original_swap_chain_get_back_buffer)return D3DERR_INVALIDCALL;
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_original_swap_chain_get_back_buffer(chain,index,type,out);
    const auto rva=CallerRva(origin);
    if(rva && SUCCEEDED(hr)){
        const auto owner=OriginalSwapChainDeviceOwner(chain);
        if(owner==OriginalSwapChainOwner::Primary){
            g_original_swap_chain_calls[3].fetch_add(1u,std::memory_order_relaxed);
            if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
                OriginalSwapChainCoverageUnknown(rva);
        }else if(owner==OriginalSwapChainOwner::Unknown){
            OriginalSwapChainCoverageUnknown(rva);
        }
    }
    return hr;
}
bool EnsureOriginalSwapChainObserved(IDirect3DSwapChain9* chain) {
    if(!chain)return false;
    auto** table=*reinterpret_cast<void***>(chain);
    if(!table || !table[3] || !table[4] || !table[5])return false;
    std::lock_guard lock(g_original_swap_chain_hook_mutex);
    if(g_original_swap_chain_present_target ||
       g_original_swap_chain_get_front_target ||
       g_original_swap_chain_get_back_target)
        return g_original_swap_chain_present_target==table[3] &&
               g_original_swap_chain_get_front_target==table[4] &&
               g_original_swap_chain_get_back_target==table[5] &&
               g_original_swap_chain_present &&
               g_original_swap_chain_get_front_buffer_data &&
               g_original_swap_chain_get_back_buffer;
    const bool present=Hook(table[3],
        reinterpret_cast<void*>(&OriginalSwapChainPresentHook),
        g_original_swap_chain_present);
    const bool front=Hook(table[4],
        reinterpret_cast<void*>(&OriginalSwapChainGetFrontBufferDataHook),
        g_original_swap_chain_get_front_buffer_data);
    const bool back=Hook(table[5],
        reinterpret_cast<void*>(&OriginalSwapChainGetBackBufferHook),
        g_original_swap_chain_get_back_buffer);
    if(!(present&&front&&back)){
        if(present){MH_DisableHook(table[3]);MH_RemoveHook(table[3]);}
        if(front){MH_DisableHook(table[4]);MH_RemoveHook(table[4]);}
        if(back){MH_DisableHook(table[5]);MH_RemoveHook(table[5]);}
        g_original_swap_chain_present=nullptr;
        g_original_swap_chain_get_front_buffer_data=nullptr;
        g_original_swap_chain_get_back_buffer=nullptr;
        return false;
    }
    g_original_swap_chain_present_target=table[3];
    g_original_swap_chain_get_front_target=table[4];
    g_original_swap_chain_get_back_target=table[5];
    return true;
}
HRESULT STDMETHODCALLTYPE OriginalCreateAdditionalSwapChainHook(
    IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* pp,
    IDirect3DSwapChain9** out){
    if(!g_create_additional_swap_chain)return D3DERR_INVALIDCALL;
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_create_additional_swap_chain(device,pp,out);
    if(g_primary_device.Owns(device) && SUCCEEDED(hr)){
        g_original_swap_chain_calls[0].fetch_add(1u,std::memory_order_relaxed);
        if(!out || !*out || !EnsureOriginalSwapChainObserved(*out))
            OriginalSwapChainCoverageUnknown(CallerRva(origin));
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalGetSwapChainHook(
    IDirect3DDevice9* device,UINT index,IDirect3DSwapChain9** out){
    if(!g_get_swap_chain)return D3DERR_INVALIDCALL;
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_get_swap_chain(device,index,out);
    if(g_primary_device.Owns(device) && SUCCEEDED(hr)){
        g_original_swap_chain_calls[1].fetch_add(1u,std::memory_order_relaxed);
        if(!out || !*out || !EnsureOriginalSwapChainObserved(*out))
            OriginalSwapChainCoverageUnknown(CallerRva(origin));
    }
    return hr;
}
