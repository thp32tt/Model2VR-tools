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
    bool device_hook_installed,bool /*native_gpu_replay_enabled*/) noexcept {
    // An original frame ID alone never authenticates a world draw.
    // The independently proven CPU source is arbitrated at physical Present.
    if(!device_hook_installed ||
       original_present_counter==0xffffffffu)return 0u;
    return std::uint64_t{original_present_counter}+1u;
}
// Never replace GPU replay with a foreign-thread, unarmed or stale CPU frame.
inline bool PreferPendingCpuWorld(std::uint64_t pending_frame,
    std::uint64_t original_frame,bool armed,bool original_render_thread) noexcept {
    return armed && original_render_thread && pending_frame!=0u &&
           pending_frame==original_frame;
}
// The CPU world may run alone, be excluded by a GPU-owned frame, or be
// preferred over GPU replay with the original backbuffer reserved for failure.
enum class OriginalStereoSourceMode : std::uint32_t {
    CpuOnly=0u, GpuOnly=1u, CpuPreferredGpuFallback=2u
};
inline constexpr OriginalStereoSourceMode SelectOriginalStereoSourceMode(
    bool gpu_native_replay,bool verified_cpu_pending) noexcept {
    return !gpu_native_replay?OriginalStereoSourceMode::CpuOnly:
           verified_cpu_pending?OriginalStereoSourceMode::CpuPreferredGpuFallback:
                                OriginalStereoSourceMode::GpuOnly;
}
// The original D3D9 Present is a physical one-way lifetime boundary.
// An active, unarmed CPU-world eye pair whose Present has arrived can
// never be completed after its original screen frame is already over.
// An armed pair older than the source Present has likewise expired.
// This is independent of world/HUD classification and does not affect
// the authentic D3D9 Present, GPU replay or original-mono fallback.
enum class OriginalWorldPresentExpiry : std::uint32_t {
    None=0u, Unarmed=1u, StaleArmed=2u
};
inline constexpr OriginalWorldPresentExpiry DecideOriginalWorldPresentExpiry(
    std::uint64_t source_frame,std::uint64_t original_frame,
    bool active,bool armed) noexcept {
    if(!active && !armed)return OriginalWorldPresentExpiry::None;
    if(!source_frame || !original_frame)
        return active?OriginalWorldPresentExpiry::Unarmed:
                      OriginalWorldPresentExpiry::StaleArmed;
    if(source_frame>original_frame)return OriginalWorldPresentExpiry::None;
    if(active)return OriginalWorldPresentExpiry::Unarmed;
    return source_frame<original_frame?
        OriginalWorldPresentExpiry::StaleArmed:
        OriginalWorldPresentExpiry::None;
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
