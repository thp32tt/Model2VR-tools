#pragma once
// Final, post-upload validation of ORIGINAL x86 D3D9 game pixels before
// Win64 OpenXR xrEndFrame. The source can issue a newer FAILED physical
// Present or die WHILE the host uploads up to 8 MiB into two XR swapchains.
// Prior Peek/copy approval is not an irrevocable permission to display.
// NO emulator state, ROM pixels, CPU-world/6DoF proof or HMD dependency.
#include "daytona_original_present_receipt.hpp"
#include "stereo_frame_policy.hpp"
#include "daytona_xr_host_epoch_gate.hpp"
#include <cstdint>

namespace m2vr::xrcommit {
enum class Reject : std::uint8_t {
    None, SourceDead, StalePixels, PayloadMismatch, PoseUnproven,
    MissingReceipt, NewerFailedPresent, ContradictoryReceipt,
    PreviousHostGeneration, PreviousXrSessionGeneration
};
inline constexpr const char* Name(Reject reason) noexcept {
    switch(reason){
    case Reject::None: return "ORIGINAL_XR_LAYER_SUBMITTED";
    case Reject::SourceDead: return "ORIGINAL_X86_PROCESS_TERMINATED";
    case Reject::StalePixels: return "ORIGINAL_EYE_FRAME_QPC_EXPIRED";
    case Reject::PayloadMismatch: return "ORIGINAL_EYE_BUFFER_FRAME_MISMATCH";
    case Reject::PoseUnproven: return "ORIGINAL_OPENXR_EYE_POSE_UNPROVEN";
    case Reject::MissingReceipt: return "ORIGINAL_PRESENT_RECEIPT_MISSING";
    case Reject::NewerFailedPresent: return "NEWER_ORIGINAL_D3D9_PRESENT_FAILED";
    case Reject::ContradictoryReceipt: return "ORIGINAL_PRESENT_RECEIPT_CONTRADICTORY";
    case Reject::PreviousHostGeneration: return "ORIGINAL_EYES_FROM_PREVIOUS_XR_HOST";
    case Reject::PreviousXrSessionGeneration: return "ORIGINAL_EYES_FROM_PREVIOUS_XR_SESSION";
    }
    return "UNKNOWN_ORIGINAL_XR_LAYER_REJECTION";
}
struct Result {
    bool submit=false;
    Reject reason=Reject::MissingReceipt;
};
inline Result VerifyOriginalFrameAtEnd(
    bool original_source_alive,
    const transport::StereoFrameGate& accepted,
    const transport::Snapshot& eyes,
    bool exact_source_pose_accepted,
    std::uint64_t final_qpc,std::uint64_t qpc_hz,
    const producerhealth::Snapshot& final_health,
    bool final_health_read,
    std::uint64_t host_start_qpc,
    std::uint64_t session_start_qpc) noexcept {
    using namespace transport;
    if(!original_source_alive)return {false,Reject::SourceDead};
    if(!accepted.Cached(final_qpc,qpc_hz))
        return {false,Reject::StalePixels};
    // Recheck at xrEndFrame, AFTER potentially slow D3D11 image uploads.
    if(!xrepoch::WithinCurrentHost(eyes.timestamp_qpc,host_start_qpc))
        return {false,Reject::PreviousHostGeneration};
    if(!xrepoch::WithinActiveSession(eyes.timestamp_qpc,host_start_qpc,
                                    session_start_qpc))
        return {false,Reject::PreviousXrSessionGeneration};
    if(!eyes.frame_id || eyes.frame_id!=accepted.id() ||
       eyes.kind!=accepted.kind() ||
       !ValidEyePayload(eyes.kind,eyes.eyes[0].size(),
                        eyes.eyes[1].size()))
        return {false,Reject::PayloadMismatch};
    if(IsStereoGame(eyes.kind) &&
       (!exact_source_pose_accepted || eyes.pose_sequence==0u))
        return {false,Reject::PoseUnproven};
    if(!IsStereoGame(eyes.kind) && eyes.pose_sequence!=0u)
        return {false,Reject::PoseUnproven};
    // Diagnostics use source IPC but NEVER assert original D3D9 imagery.
    if(eyes.kind==FrameKind::Diagnostic)
        return {true,Reject::None};
    // At the moment of actual compositor submission, ensure the producer's
    // genuine physical Present receipt is still readable and fresh. An odd
    // seqlock or a dead x86 writer is NOT permission to hold old game eyes.
    if(!final_health_read ||
       !producerhealth::Fresh(final_health,final_qpc,qpc_hz) ||
       final_health.frame_id<eyes.frame_id)
        return {false,Reject::MissingReceipt};
    // A physically *newer* original Present cannot predate the copied
    // D3D9 eye surface in the shared Windows QPC domain. This also covers
    // bogus zero-reason "success" receipts used to keep stale eyes alive.
    if(!eyes.timestamp_qpc ||
       final_health.timestamp_qpc<eyes.timestamp_qpc)
        return {false,Reject::ContradictoryReceipt};
    if(final_health.frame_id==eyes.frame_id){
        return receipt::Authorize(eyes.frame_id,eyes.kind,
             eyes.timestamp_qpc,final_health,true)==receipt::Verdict::Admit
            ?Result{true,Reject::None}
            :Result{false,Reject::ContradictoryReceipt};
    }
    // This cached pair WAS already accepted by the pre-copy/post-copy
    // physical Present gate. A newer successful Present can briefly hold
    // it (60-Hz original vs 90/120-Hz HMD), but a newer failed Present or
    // stereo->original-mono transition must revoke it immediately.
    // A purported newer reason=0 success with ZERO authentic scene Draws
    // cannot act as a harmless "success" while retaining previous eyes.
    if(receipt::RevokesCached(eyes.frame_id,eyes.kind,eyes.timestamp_qpc,
                            final_health,true) ||
       (final_health.reason_mask==0u &&
         (final_health.verified_scene_draws==0u ||
          producerhealth::SumFamilies(final_health.family_draws)!=
             final_health.mirrored_draws)))
        return {false,Reject::NewerFailedPresent};
    return {true,Reject::None};
}
} // namespace m2vr::xrcommit
