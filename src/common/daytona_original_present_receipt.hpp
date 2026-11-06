#pragma once
// Original x86 physical D3D9 Present -> Win64 OpenXR frame authorization.
// The shared frame payload and the producer-health receipt are separate
// seqlocks. An eye payload alone is never evidence that its *same* original
// Present succeeded. This policy is intentionally pointer-free and does not
// promote scene/camera evidence or enable unverified World3D.
#include "daytona_producer_health.hpp"
#include "stereo_frame_ipc.h"
#include <cstdint>

namespace m2vr::receipt {
enum class Verdict : std::uint8_t {
    AwaitHealth, Admit, Reject
};

// A frame may be copied only when the health report for the EXACT same
// physical original D3D9 Present has arrived and agreed with its source kind.
// Health from an older frame is a pending asynchronous receipt, not success;
// a newer report has overwritten this frame's receipt and cannot attest it.
// Diagnostic pixels never claim to be original game imagery.
inline constexpr Verdict Authorize(
    std::uint64_t frame,transport::FrameKind kind,
    std::uint64_t source_qpc,
    const producerhealth::Snapshot& health,bool fresh) noexcept {
    if(!frame || !transport::ValidSourceKind(kind))return Verdict::Reject;
    if(kind==transport::FrameKind::Diagnostic)return Verdict::Admit;
    if(!fresh || health.frame_id<frame)return Verdict::AwaitHealth;
    if(health.frame_id>frame)return Verdict::Reject;
    // The native x86 producer writes the actual eye payload BEFORE the
    // original physical Present receipt. Both processes share Windows QPC.
    // A purported successful receipt timestamped before the pixels cannot
    // attest them, even when numeric frame IDs happen to match.
    if(!source_qpc || !health.timestamp_qpc ||
       health.timestamp_qpc<source_qpc)return Verdict::Reject;
    // The original x86 producer reports GPU replay and CPU-projector
    // outcomes differently. A zero reason mask alone MUST NOT let a
    // repackaged GPU eye masquerade as CPU World3D (or vice versa).
    // GPU success has original mirrored Draw families and an authentic
    // scene. The verified CPU source sends no mirrored GPU family.
    if(kind==transport::FrameKind::GameStereo)
        return health.reason_mask==0u &&
               health.verified_scene_draws!=0u &&
               health.mirrored_draws!=0u &&
               producerhealth::SumFamilies(health.family_draws)==
                   health.mirrored_draws ? Verdict::Admit : Verdict::Reject;
    if(kind==transport::FrameKind::VerifiedCpuWorldStereo)
        return health.reason_mask==0u &&
               health.verified_scene_draws!=0u &&
               health.mirrored_draws==0u && health.mirrored_clears==0u &&
               producerhealth::SumFamilies(health.family_draws)==0u
                   ? Verdict::Admit : Verdict::Reject;
    if(transport::IsMonoGame(kind))
        return producerhealth::OriginalMonoPublished(health.reason_mask)
            ? Verdict::Admit : Verdict::Reject;
    return Verdict::Reject;
}

// Once admitted, a later same-frame contradictory health status must also
// revoke the cached pair. A newer failed Present uses existing bounded
// fallback logic. A newer successful Present may hold the prior verified
// pair briefly while its own IPC copy is pending, subject to the 125/250ms
// frame-age gate and exact pose checks in the host.
inline constexpr bool RevokesCached(
    std::uint64_t frame,transport::FrameKind kind,
    std::uint64_t source_qpc,
    const producerhealth::Snapshot& health,bool fresh) noexcept {
    if(!fresh || !frame || kind==transport::FrameKind::Diagnostic)
        return false;
    if(health.frame_id==frame)
        return Authorize(frame,kind,source_qpc,health,true)!=Verdict::Admit;
    if(health.frame_id>frame)
        return producerhealth::InvalidatesCachedPair(
            health,true,frame,transport::IsStereoGame(kind));
    return false;
}
} // namespace m2vr::receipt
