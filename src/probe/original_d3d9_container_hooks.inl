// Original EXE-owned Surface9::GetContainer(11) / Volume9::GetContainer(7).
// The caller may obtain an unobserved texture or SwapChain parent and mutate
// it without any later SetTexture/GetSwapChain. Register its authentic COM
// implementation before that alias reaches the caller. No source data, COM
// pointer, IID, handle or game pixels are written to logs or Win64 IPC.
void OriginalTextureCoverageUnknown(std::uint32_t caller_rva) noexcept;
bool ObserveOriginalBoundTexture(IDirect3DBaseTexture9*,std::uint32_t);
bool EnsureOriginalSwapChainObserved(IDirect3DSwapChain9*);
bool PrimaryOriginalVolume(IDirect3DVolume9*) noexcept;

bool ObserveOriginalContainerTexture(IDirect3DBaseTexture9* texture,
                                     std::uint32_t rva){
    return texture && PrimaryOriginalResource(texture) &&
           ObserveOriginalBoundTexture(texture,rva);
}
bool ObserveOriginalContainerSwapChain(IDirect3DSwapChain9* chain){
    if(!chain)return false;
    IDirect3DDevice9* device=nullptr;
    const HRESULT hr=chain->GetDevice(&device);
    const bool primary=SUCCEEDED(hr)&&device&&g_primary_device.Owns(device);
    if(device)device->Release();
    return primary && EnsureOriginalSwapChainObserved(chain);
}
// Accept the exact requested interface; a returned IUnknown/Resource alias
// is checked with balanced QueryInterface/Release, never cast blindly to a
// concrete texture or SwapChain. Unknown parent types fail closed for XR.
bool ObserveOriginalContainerAlias(REFIID iid,void* object,
                                   std::uint32_t rva){
    if(!object || !rva)return false;
    if(IsEqualIID(iid,IID_IDirect3DTexture9))
        return ObserveOriginalContainerTexture(
            static_cast<IDirect3DTexture9*>(object),rva);
    if(IsEqualIID(iid,IID_IDirect3DCubeTexture9))
        return ObserveOriginalContainerTexture(
            static_cast<IDirect3DCubeTexture9*>(object),rva);
    if(IsEqualIID(iid,IID_IDirect3DVolumeTexture9))
        return ObserveOriginalContainerTexture(
            static_cast<IDirect3DVolumeTexture9*>(object),rva);
    if(IsEqualIID(iid,IID_IDirect3DBaseTexture9))
        return ObserveOriginalContainerTexture(
            static_cast<IDirect3DBaseTexture9*>(object),rva);
    if(IsEqualIID(iid,IID_IDirect3DSwapChain9))
        return ObserveOriginalContainerSwapChain(
            static_cast<IDirect3DSwapChain9*>(object));
    if(IsEqualIID(iid,IID_IDirect3DDevice9))
        return g_primary_device.Owns(object);
    if(!IsEqualIID(iid,IID_IUnknown) &&
       !IsEqualIID(iid,IID_IDirect3DResource9))return false;

    auto* alias=static_cast<IUnknown*>(object);
    IDirect3DBaseTexture9* texture=nullptr;
    const HRESULT tex_hr=alias->QueryInterface(
        IID_IDirect3DBaseTexture9,reinterpret_cast<void**>(&texture));
    if(SUCCEEDED(tex_hr) && texture){
        const bool ok=ObserveOriginalContainerTexture(texture,rva);
        texture->Release();
        return ok;
    }
    if(texture)texture->Release();
    IDirect3DSwapChain9* chain=nullptr;
    const HRESULT swap_hr=alias->QueryInterface(
        IID_IDirect3DSwapChain9,reinterpret_cast<void**>(&chain));
    if(SUCCEEDED(swap_hr) && chain){
        const bool ok=ObserveOriginalContainerSwapChain(chain);
        chain->Release();
        return ok;
    }
    if(chain)chain->Release();
    IDirect3DDevice9* device=nullptr;
    const HRESULT device_hr=alias->QueryInterface(
        IID_IDirect3DDevice9,reinterpret_cast<void**>(&device));
    const bool known=SUCCEEDED(device_hr)&&device&&g_primary_device.Owns(device);
    if(device)device->Release();
    return known;
}
HRESULT STDMETHODCALLTYPE OriginalSurfaceGetContainerHook(
    IDirect3DSurface9* surface,REFIID iid,void** out){
    if(!g_surface_get_container)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalResource(surface);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_surface_get_container(surface,iid,out);
    const auto rva=primary?CallerRva(caller):0u;
    // A failed GetContainer is an ordinary read-only query; do not poison
    // eye sources or substitute a fabricated HRESULT.
    if(rva && SUCCEEDED(hr) &&
       (!out || !ObserveOriginalContainerAlias(iid,*out,rva)))
        OriginalTextureCoverageUnknown(rva);
    return hr;
}
HRESULT STDMETHODCALLTYPE OriginalVolumeGetContainerHook(
    IDirect3DVolume9* volume,REFIID iid,void** out){
    if(!g_volume_get_container)return D3DERR_INVALIDCALL;
    const bool primary=PrimaryOriginalVolume(volume);
    void* const caller=_ReturnAddress();
    const HRESULT hr=g_volume_get_container(volume,iid,out);
    const auto rva=primary?CallerRva(caller):0u;
    if(rva && SUCCEEDED(hr) &&
       (!out || !ObserveOriginalContainerAlias(iid,*out,rva)))
        OriginalTextureCoverageUnknown(rva);
    return hr;
}
