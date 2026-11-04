#pragma once
// An optional, separately authenticated CPU-world frame may be submitted
// only after its matching ORIGINAL D3D9 Present succeeds. Legacy immediate
// submission remains available to source adapters that require that ABI.
// No D3D9 Draw, simulator, steering or FFB callback is called here.
#include <cstdint>

namespace m2vr::vrbridge {
enum class PendingWorldPresent : std::uint8_t {
    None, Wait, Drop, Publish
};
inline PendingWorldPresent DecidePendingWorldPresent(
    std::uint64_t pending_frame,std::uint64_t original_present_frame,
    bool original_present_ok,bool native_gpu_replay_enabled,
    bool original_render_thread) noexcept {
    if(!pending_frame)return PendingWorldPresent::None;
    if(original_present_frame<pending_frame)
        return PendingWorldPresent::Wait;
    if(original_present_frame!=pending_frame ||
       !original_present_ok || native_gpu_replay_enabled ||
       !original_render_thread)
        return PendingWorldPresent::Drop;
    return PendingWorldPresent::Publish;
}
} // namespace m2vr::vrbridge
