// Genuine primary original Device9 presentation controls: HRESULT-returning
// SetDialogBoxMode (COM slot 20) and void SetGammaRamp (COM slot 21).
// An original dialogue/GDI overlay or display gamma mutation is not part
// of the original Draw material/CPU-world camera. Revoke both speculative
// XR eyes BEFORE the real COM call. Never inspect/serialize gamma ramp data,
// ROM, pixels or original game pointers; original mono/FFB/input unchanged.
void BeforeOriginalPresentationControl(
    std::uint32_t caller_rva,std::uint32_t method) noexcept {
    if(method<1u || method>5u)return;
    Model2VR_ObserveOriginalD3D9PresentationMutation(
        g_present_count.load(std::memory_order_acquire)+1u,caller_rva,
        method>=3u?0u:g_original_frame_draw_ordinal.load(
            std::memory_order_acquire),method);
}
HRESULT STDMETHODCALLTYPE OriginalSetDialogBoxModeHook(
    IDirect3DDevice9* device,BOOL enabled){
    if(!g_original_set_dialog_box_mode)return D3DERR_INVALIDCALL;
    if(!g_primary_device.Owns(device))
        return g_original_set_dialog_box_mode(device,enabled);
    void* const origin=_ReturnAddress();
    const auto rva=CallerRva(origin);
    if(rva)BeforeOriginalPresentationControl(rva,1u);
    else {
        g_original_external_presentation_unproven.store(
            true,std::memory_order_release);
        BeforeOriginalPresentationControl(0u,5u);
    }
    const HRESULT hr=g_original_set_dialog_box_mode(device,enabled);
    if(rva && FAILED(hr))TrackFailedOriginalD3D9Surface(hr,70u,rva);
    if(rva && SUCCEEDED(hr)){
        g_original_dialog_box_active.store(enabled!=FALSE,
                                           std::memory_order_release);
        g_original_presentation_controls[0].fetch_add(
            1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::DeviceDialogBoxMode,rva,hr);
    }
    return hr;
}
void STDMETHODCALLTYPE OriginalSetGammaRampHook(
    IDirect3DDevice9* device,UINT swap_chain,DWORD flags,
    const D3DGAMMARAMP* ramp){
    if(!g_original_set_gamma_ramp)return;
    if(!g_primary_device.Owns(device)){
        g_original_set_gamma_ramp(device,swap_chain,flags,ramp);
        return;
    }
    void* const origin=_ReturnAddress();
    const auto rva=CallerRva(origin);
    if(!rva){
        g_original_external_presentation_unproven.store(
            true,std::memory_order_release);
        g_original_gamma_unproven.store(true,std::memory_order_release);
        BeforeOriginalPresentationControl(0u,5u);
    }
    if(rva){
        // A void original COM method cannot report failure. Keep the source
        // gamma unproven until a new process; Reset is not gamma calibration.
        g_original_gamma_unproven.store(true,std::memory_order_release);
        BeforeOriginalPresentationControl(rva,2u);
    }
    // Original COM executes once, without changing ramp bytes or flags.
    g_original_set_gamma_ramp(device,swap_chain,flags,ramp);
    if(rva){
        g_original_presentation_controls[1].fetch_add(
            1u,std::memory_order_relaxed);
        RecordOriginalLineageWrite(
            m2vr::drawprobe::OriginalLineageKind::DeviceGammaRamp,rva,S_OK);
    }
}
