#pragma once
// One physical original Daytona x86 D3D9 Present -> one Win64 OpenXR
// binocular source transaction. No original pixels or world claims here.
// Two independent shared mappings (frame payload and post-Present health)
// must attest the EXACT same stamped pair, not adjacent valid frames.
#include "daytona_original_present_receipt.hpp"
#include "stereo_frame_policy.hpp"
#include "daytona_xr_host_epoch_gate.hpp"

namespace m2vr::handoff {
inline constexpr bool SameOriginalPresentPayload(
    const transport::FrameStamp& inspected,
    const transport::Snapshot& copied) noexcept {
    // The producer may advance from Peek to the expensive 8-MiB copy.
    // Do NOT silently accept a different frame, source kind or XR pose.
    // A later XR display tick may retry the newer source normally.
    return inspected.frame_id!=0u &&
        inspected.frame_id==copied.frame_id &&
        inspected.timestamp_qpc!=0u &&
        inspected.timestamp_qpc==copied.timestamp_qpc &&
        inspected.kind==copied.kind &&
        inspected.pose_sequence==copied.pose_sequence &&
        transport::ValidSourceKind(copied.kind) &&
        transport::ValidEyePayload(copied.kind,
            copied.eyes[0].size(),copied.eyes[1].size());
}
inline constexpr bool AdmitOriginalPresentToXr(
    const transport::FrameStamp& inspected,
    const transport::Snapshot& copied,
    const producerhealth::Snapshot& post_copy_health,
    bool post_copy_health_fresh,bool source_pose_in_host_ring,
    std::uint64_t host_start_qpc) noexcept {
    // Even the same physical Present and source pose tag cannot authenticate
    // a source packet copied from a previous Win64 OpenXR host process.
    return xrepoch::WithinCurrentHost(inspected.timestamp_qpc,host_start_qpc) &&
        xrepoch::WithinCurrentHost(copied.timestamp_qpc,host_start_qpc) &&
        SameOriginalPresentPayload(inspected,copied) &&
        transport::SourcePoseMatchesFrame(
            copied.kind,copied.pose_sequence,source_pose_in_host_ring) &&
        receipt::Authorize(copied.frame_id,copied.kind,
            copied.timestamp_qpc,post_copy_health,
            post_copy_health_fresh)==receipt::Verdict::Admit;
}
} // namespace m2vr::handoff
