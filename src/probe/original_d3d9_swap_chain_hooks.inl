// Genuine primary original x86 D3D9 additional SwapChain boundary.
// Device CreateAdditionalSwapChain(13)/GetSwapChain(14) return an object
// whose Present(3) has its OWN physical presentation boundary. Never
// confuse it with primary IDirect3DDevice9::Present(17) or mint a World3D
// source from a read-only GetBackBuffer(5). All original COM calls run once.
// A surface returned here receives the existing CPU/GDI write observer before
// its first LockRect/GetDC, with no pixels or COM identities logged.
bool OriginalSwapChainBelongsToPrimary(IDirect3DSwapChain9* chain) noexcept {
    if(!chain)return false;
    IDirect3DDevice9* device=nullptr;
    const HRESULT hr=chain->GetDevice(&device);
    const bool primary=SUCCEEDED(hr)&&device&&g_primary_device.Owns(device);
    if(device)device->Release();
    return primary;
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
        if(OriginalSwapChainBelongsToPrimary(chain)){
            g_original_swap_chain_calls[2].fetch_add(1u,std::memory_order_relaxed);
            // Revoke CPU-world and GPU-eye speculation BEFORE genuine
            // presentation. Alternate Present has no authenticated frame id.
            OriginalSwapChainCoverageUnknown(rva);
            Model2VR_RejectOriginalD3D9SwapChainPresent(
                g_present_count.load(std::memory_order_acquire)+1u,rva,1u);
        }else{
            // An EXE-owned SwapChain with uncertain owner must never
            // silently masquerade as the primary stereo source.
            OriginalSwapChainCoverageUnknown(rva);
        }
    }
    return g_original_swap_chain_present(chain,src,dst,window,dirty,flags);
}
HRESULT STDMETHODCALLTYPE OriginalSwapChainGetBackBufferHook(
    IDirect3DSwapChain9* chain,UINT index,D3DBACKBUFFER_TYPE type,
    IDirect3DSurface9** out){
    if(!g_original_swap_chain_get_back_buffer)return D3DERR_INVALIDCALL;
    void* const origin=_ReturnAddress();
    const HRESULT hr=g_original_swap_chain_get_back_buffer(chain,index,type,out);
    const auto rva=CallerRva(origin);
    if(rva && OriginalSwapChainBelongsToPrimary(chain) && SUCCEEDED(hr)){
        g_original_swap_chain_calls[3].fetch_add(1u,std::memory_order_relaxed);
        if(!out || !*out || !ObserveOriginalTextureSurface(*out,rva))
            OriginalSwapChainCoverageUnknown(rva);
    }
    return hr;
}
bool EnsureOriginalSwapChainObserved(IDirect3DSwapChain9* chain) {
    if(!chain)return false;
    auto** table=*reinterpret_cast<void***>(chain);
    if(!table || !table[3] || !table[5])return false;
    std::lock_guard lock(g_original_swap_chain_hook_mutex);
    if(g_original_swap_chain_present_target ||
       g_original_swap_chain_get_back_target)
        return g_original_swap_chain_present_target==table[3] &&
               g_original_swap_chain_get_back_target==table[5] &&
               g_original_swap_chain_present &&
               g_original_swap_chain_get_back_buffer;
    const bool present=Hook(table[3],
        reinterpret_cast<void*>(&OriginalSwapChainPresentHook),
        g_original_swap_chain_present);
    const bool back=Hook(table[5],
        reinterpret_cast<void*>(&OriginalSwapChainGetBackBufferHook),
        g_original_swap_chain_get_back_buffer);
    if(!(present&&back)){
        if(present){MH_DisableHook(table[3]);MH_RemoveHook(table[3]);}
        if(back){MH_DisableHook(table[5]);MH_RemoveHook(table[5]);}
        g_original_swap_chain_present=nullptr;
        g_original_swap_chain_get_back_buffer=nullptr;
        return false;
    }
    g_original_swap_chain_present_target=table[3];
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
