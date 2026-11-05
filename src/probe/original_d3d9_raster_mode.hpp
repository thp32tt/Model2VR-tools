#pragma once
// Persistent authentic D3D9 raster-mode state across physical Presents.
#include <atomic>
#include <cmath>
#include <cstdint>
namespace m2vr::drawprobe {
class OriginalRasterModeState final {
public:
    static constexpr std::uint32_t kSoftwareVertex=1u,kNPatch=2u;
    void SuccessfulSoftwareVertexWrite(bool enabled)noexcept { Set(kSoftwareVertex,enabled); }
    void SuccessfulNPatchWrite(float segments)noexcept {
        const bool safe=std::isfinite(segments) &&
            (segments==0.0f || segments==1.0f);
        Set(kNPatch,!safe);
    }
    std::uint32_t UnsupportedMask()const noexcept {
        return bits_.load(std::memory_order_acquire);
    }
    void AfterSuccessfulDeviceReset()noexcept {
        bits_.store(0u,std::memory_order_release);
    }
private:
    void Set(std::uint32_t bit,bool enabled)noexcept {
        if(enabled)bits_.fetch_or(bit,std::memory_order_acq_rel);
        else bits_.fetch_and(~bit,std::memory_order_acq_rel);
    }
    std::atomic<std::uint32_t> bits_{0u};
};
} // namespace m2vr::drawprobe
