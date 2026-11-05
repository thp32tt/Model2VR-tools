#pragma once
// Persistent unknown processed-VB contents after real ProcessVertices.
// Source bytes, COM pointers and patch handles are never copied or logged.
#include <atomic>
namespace m2vr::drawprobe {
class OriginalUnmirroredGeometryState final {
public:
    void SuccessfulProcessVertices() noexcept {
        processed_.store(true,std::memory_order_release);
    }
    bool HasProcessedVertices() const noexcept {
        return processed_.load(std::memory_order_acquire);
    }
    void AfterSuccessfulDeviceReset() noexcept {
        processed_.store(false,std::memory_order_release);
    }
private:
    std::atomic<bool> processed_{false};
};
} // namespace m2vr::drawprobe
