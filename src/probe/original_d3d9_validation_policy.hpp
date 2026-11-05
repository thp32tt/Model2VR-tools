#pragma once
// Original IDirect3DDevice9::ValidateDevice (COM 70) returns a count of
// required hardware rendering passes. This is a source-equivalence gate,
// NOT gameplay camera calibration or permission to enable World3D.
// No Windows headers or game-private state; pure constexpr policy.
#include <cstdint>
namespace m2vr::vrbridge {
enum class OriginalDeviceValidationDecision : std::uint32_t {
    TrustedSinglePass=0u, OriginalFailure=1u,
    MultiplePasses=2u, ResultUnknown=3u
};
constexpr OriginalDeviceValidationDecision DecideOriginalD3D9Validation(
    std::uint32_t original_hresult_bits,bool count_readable,
    std::uint32_t original_pass_count) noexcept {
    if(original_hresult_bits & 0x80000000u)
        return OriginalDeviceValidationDecision::OriginalFailure;
    if(original_hresult_bits!=0u || !count_readable || !original_pass_count)
        return OriginalDeviceValidationDecision::ResultUnknown;
    return original_pass_count==1u ?
        OriginalDeviceValidationDecision::TrustedSinglePass :
        OriginalDeviceValidationDecision::MultiplePasses;
}
} // namespace m2vr::vrbridge
