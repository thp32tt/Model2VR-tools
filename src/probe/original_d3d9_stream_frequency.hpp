#pragma once
// Persistent original D3D9 stream-frequency provenance across Presents.
// Non-unit dividers/instancing cannot be silently treated as ordinary VB.
#include <atomic>
#include <cstdint>

namespace m2vr::drawprobe {
class OriginalStreamFrequencyState final {
public:
    static constexpr unsigned kTrackedStreams=16u;
    static constexpr std::uint32_t kUnknownStream=1u<<31;
    void SuccessfulOriginalWrite(unsigned stream,unsigned setting)noexcept{
        if(stream>=kTrackedStreams){
            nonunit_mask_.fetch_or(kUnknownStream,std::memory_order_acq_rel);
        }else{
            const auto bit=std::uint32_t{1}<<stream;
            if(setting==1u)
                nonunit_mask_.fetch_and(~bit,std::memory_order_acq_rel);
            else
                nonunit_mask_.fetch_or(bit,std::memory_order_acq_rel);
        }
    }
    std::uint32_t UnsupportedMask()const noexcept{
        return nonunit_mask_.load(std::memory_order_acquire);
    }
    void AfterSuccessfulDeviceReset()noexcept{
        nonunit_mask_.store(0u,std::memory_order_release);
    }
private:
    std::atomic<std::uint32_t> nonunit_mask_{0u};
};
} // namespace m2vr::drawprobe
