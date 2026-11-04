#pragma once
// An optional, separately authenticated CPU-world frame may be submitted
// only after its matching ORIGINAL D3D9 Present succeeds. Legacy immediate
// submission remains available to source adapters that require that ABI.
// No D3D9 Draw, simulator, steering or FFB callback is called here.
#include <cstdint>
#include "../common/daytona_producer_health.hpp"

namespace m2vr::vrbridge {
// A source camera adapter may join a verified world transaction to the
// actual original device frame, not to a guessed simulation tick. The
// original Present counter is incremented on entry. Fail closed on wrap.
inline std::uint64_t NextOriginalPresentFrameId(
    std::uint32_t original_present_counter,
    bool device_hook_installed,bool native_gpu_replay_enabled) noexcept {
    if(!device_hook_installed || native_gpu_replay_enabled ||
       original_present_counter==0xffffffffu)return 0u;
    return std::uint64_t{original_present_counter}+1u;
}
enum class PendingWorldPresent : std::uint8_t {
    None, Wait, Drop, Publish
};
// Use the same pointer-free producer-health ABI as GPU replay. A real
// CPU-world pair reports success only after its original Present succeeds.
inline std::uint32_t PendingWorldHealthReason(
    PendingWorldPresent action,bool original_present_ok,
    bool published,bool source_pose_live=true) noexcept {
    using namespace m2vr::producerhealth;
    if(action==PendingWorldPresent::Publish)
        return !source_pose_live ? PoseExpired : (published ? 0u : TransportFailed);
    if(!original_present_ok)return OriginalPresentFailed;
    return WrongDevice;
}
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
