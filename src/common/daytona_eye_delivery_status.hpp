#pragma once
// Cause-aware OpenXR eye-delivery state. A tracked headset or successful
// launcher handshake does NOT imply actual Daytona pixels reached both eyes.
// Pure, pointer-free x86/x64 logic: never touches the original game, Draw,
// FFB or a second simulation tick.
#include <cstdint>

namespace m2vr::delivery {
enum class Status : std::uint8_t {
    NoEmulatorPid, NoProducerHealth, NoFrameTransport, AwaitingGameFrame,
    ProducerRejected, MissingOriginalPoseTag, ExpiredOriginalPoseTag,
    SourcePairStale, DiagnosticOnly, HostEyeSizeMismatch, LiveGameStereo
};
struct Evidence {
    bool emulator_selected=false;
    bool producer_health_fresh=false;
    std::uint32_t producer_reason_mask=0;
    bool frame_transport_open=false;
    bool ever_accepted_pair=false;
    bool paired_frame_fresh=false;
    bool pair_is_game=false;
    bool swapchain_compatible=false;
    bool missing_original_pose_tag=false;
    bool expired_original_pose_tag=false;
};
constexpr Status Classify(const Evidence& e) noexcept {
    if(!e.emulator_selected)return Status::NoEmulatorPid;
    // Do not report a live original game frame just because the OpenXR
    // tracking pose is valid or the loader saw the host-ready handshake.
    if(e.paired_frame_fresh) {
        if(!e.swapchain_compatible)return Status::HostEyeSizeMismatch;
        return e.pair_is_game?Status::LiveGameStereo:Status::DiagnosticOnly;
    }
    if(e.producer_health_fresh && e.producer_reason_mask!=0u)
        return Status::ProducerRejected;
    if(e.missing_original_pose_tag)return Status::MissingOriginalPoseTag;
    if(e.expired_original_pose_tag)return Status::ExpiredOriginalPoseTag;
    if(!e.producer_health_fresh && !e.frame_transport_open)
        return Status::NoProducerHealth;
    if(!e.frame_transport_open)return Status::NoFrameTransport;
    if(!e.ever_accepted_pair)return Status::AwaitingGameFrame;
    return Status::SourcePairStale;
}
constexpr const char* Name(Status s) noexcept {
    switch(s) {
    case Status::NoEmulatorPid:return "NO_EMULATOR_PID";
    case Status::NoProducerHealth:return "NO_X86_PRODUCER_OR_FRAME_IPC";
    case Status::NoFrameTransport:return "NO_GAME_FRAME_IPC";
    case Status::AwaitingGameFrame:return "AWAITING_ORIGINAL_D3D9_EYE_PAIR";
    case Status::ProducerRejected:return "ORIGINAL_D3D9_PRODUCER_REJECTED";
    case Status::MissingOriginalPoseTag:return "NO_ORIGINAL_EYE_POSE_TAG";
    case Status::ExpiredOriginalPoseTag:return "ORIGINAL_EYE_POSE_EXPIRED";
    case Status::SourcePairStale:return "GAME_EYE_PAIR_STALE_OR_REJECTED";
    case Status::DiagnosticOnly:return "DIAGNOSTIC_NOT_GAME_PIXELS";
    case Status::HostEyeSizeMismatch:return "OPENXR_EYE_SIZE_MISMATCH";
    case Status::LiveGameStereo:return "LIVE_GAME_STEREO_NOT_CAMERA_VERIFIED";
    }
    return "UNKNOWN";
}
constexpr const char* NextAction(Status s) noexcept {
    switch(s) {
    case Status::NoEmulatorPid:return "Launch with the original emulator PID";
    case Status::NoProducerHealth:return "Check original x86 hook injection, device and Present";
    case Status::NoFrameTransport:return "Check x86 stereo writer IPC creation";
    case Status::AwaitingGameFrame:return "Check original D3D9 Draw, verified scene and Present";
    case Status::ProducerRejected:return "Inspect producer reason_mask and original Draw counters";
    case Status::MissingOriginalPoseTag:return "Update x86 producer to source-pose tagged GameStereo";
    case Status::ExpiredOriginalPoseTag:return "Check emulator and OpenXR frame pacing/pose ring";
    case Status::SourcePairStale:return "Check original Present cadence and IPC read/age";
    case Status::DiagnosticOnly:return "Diagnostic eye images do not contain original game output";
    case Status::HostEyeSizeMismatch:return "Match OpenXR swapchain to original 1024x1024 eye transport";
    case Status::LiveGameStereo:return "Verify actual Daytona world/camera calibration on HMD";
    }
    return "Inspect host and original x86 producer logs";
}
class TransitionGate {
public:
    bool Changed(Status current) noexcept {
        if(seen_ && last_==current)return false;
        last_=current;seen_=true;return true;
    }
    void Reset() noexcept {seen_=false;}
private:
    Status last_=Status::AwaitingGameFrame;
    bool seen_=false;
};
} // namespace m2vr::delivery
