// Authentic primary EXE-owned IDirect3DDevice9::ValidateDevice (COM 70).
// Observe the real HRESULT and number of required rendering passes without
// replaying or changing any original D3D9 command or return value. A multi-
// pass fixed-function source cannot be equated with one speculative XR eye.
bool ReadOriginalD3D9PassCount(const UINT* passes,UINT& value) noexcept {
    if(!passes)return false;
    __try {
        value=*passes;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){
        return false;
    }
}
void OriginalValidationSourceBoundary(std::uint32_t rva,
                                      std::uint32_t reason) noexcept {
    const auto frame=static_cast<std::uint64_t>(
        g_present_count.load(std::memory_order_acquire))+1u;
    if(g_original_validation_rejected_present.load(
           std::memory_order_acquire)>=frame)return;
    // Revoke BOTH pending CPU-world and speculative GPU OpenXR eyes first;
    // only publish the dedupe marker after the source epoch is invalidated.
    Model2VR_RejectOriginalD3D9ValidationUse(
        frame,reason>=4u?0u:rva,
        reason>=4u?0u:g_original_frame_draw_ordinal.load(
            std::memory_order_acquire),reason);
    auto prior=g_original_validation_rejected_present.load(
        std::memory_order_acquire);
    while(prior<frame &&
          !g_original_validation_rejected_present.compare_exchange_weak(
              prior,frame,std::memory_order_acq_rel,
              std::memory_order_acquire)){}
}
HRESULT STDMETHODCALLTYPE OriginalValidateDeviceHook(
    IDirect3DDevice9* device,UINT* passes){
    if(!g_validate_device)return D3DERR_INVALIDCALL;
    // Nested original COM from internal eye staging already owns GPU mutex.
    if(m2vr::daytona::IsEyeReplayActive() ||
       !g_primary_device.Owns(device))
        return g_validate_device(device,passes);
    const auto caller=_ReturnAddress();
    const auto rva=CallerRva(caller);
    const HRESULT hr=g_validate_device(device,passes);
    if(!rva){
        g_original_validation_foreign_caller.store(
            true,std::memory_order_release);
        g_original_validation_unproven.store(
            true,std::memory_order_release);
        g_original_validation_counts[3].fetch_add(
            1u,std::memory_order_relaxed);
        OriginalValidationSourceBoundary(0u,4u);
        return hr;
    }
    UINT required_passes=0u;
    const bool readable=SUCCEEDED(hr) &&
        ReadOriginalD3D9PassCount(passes,required_passes);
    const auto result=m2vr::vrbridge::DecideOriginalD3D9Validation(
        static_cast<std::uint32_t>(hr),readable,required_passes);
    if(result==m2vr::vrbridge::OriginalDeviceValidationDecision::TrustedSinglePass){
        g_original_validation_counts[0].fetch_add(
            1u,std::memory_order_relaxed);
        if(!g_original_validation_foreign_caller.load(
               std::memory_order_acquire))
            g_original_validation_unproven.store(
                false,std::memory_order_release);
    }else{
        const auto reason=static_cast<std::uint32_t>(result);
        const auto index=reason==1u?2u:1u;
        g_original_validation_counts[index].fetch_add(
            1u,std::memory_order_relaxed);
        g_original_validation_unproven.store(
            true,std::memory_order_release);
        OriginalValidationSourceBoundary(rva,reason);
    }
    return hr;
}
