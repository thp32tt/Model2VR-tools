#pragma once
// Original x86 D3D9 eye-color proof before ANY 64-bit OpenXR projection
// publication. Visible native binocular pixels must differ in RGB, not
// merely in the alpha plane (render-target masks, stale clears, HUD).
// No game pixels, framebuffer fingerprints or copyrighted data are logged.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m2vr::daytona {
inline bool OriginalEyesHaveVisibleRgbDifference(
    const std::uint8_t* left,const std::uint8_t* right,
    std::size_t bytes) noexcept {
    if(!left || !right || bytes<4u || (bytes&3u)!=0u)return false;
    for(std::size_t i=0;i<bytes;i+=4u)
        if(left[i]!=right[i] || left[i+1u]!=right[i+1u] ||
           left[i+2u]!=right[i+2u])return true;
    return false;
}
inline bool OriginalEyesHaveVisibleRgbDifference(
    const std::vector<std::uint8_t>& left,
    const std::vector<std::uint8_t>& right,
    std::size_t required_bytes) noexcept {
    return required_bytes!=0u && left.size()==required_bytes &&
           right.size()==required_bytes &&
           OriginalEyesHaveVisibleRgbDifference(
               left.data(),right.data(),required_bytes);
}
} // namespace m2vr::daytona
