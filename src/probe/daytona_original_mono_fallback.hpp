#pragma once
// Original Daytona HMD fallback admission, independent of Win32/Win64.
// A real original backbuffer is a *mono* game image, never world proof.
// The x86 producer must read before original Present and publish only
// AFTER successful original Present on its actual primary D3D9 device.
#include <cstdint>
namespace m2vr::daytona {
enum class OriginalMonoFailure : std::uint32_t {
    None=0, GetBackbuffer=1, BackbufferLayout=2, ScalePlan=3,
    CreateStaging=4, CreateResolve=5, Resolve=6,
    Readback=7, Lock=8, Convert=9
};
constexpr bool CanStageOriginalMono(bool native_mode,bool original_primary,
                                    bool stereo_rejected,bool pose_live,
                                    std::uint64_t original_frame) noexcept {
    return native_mode && original_primary && stereo_rejected &&
           pose_live && original_frame!=0u;
}
constexpr bool CanPublishOriginalMono(bool staged,bool original_present_ok,
                                      bool stereo_published,bool pose_live) noexcept {
    return staged && original_present_ok && !stereo_published && pose_live;
}
} // namespace m2vr::daytona
