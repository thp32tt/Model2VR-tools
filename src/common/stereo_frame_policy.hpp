#pragma once
// Source-facing stereo host admission gate. Game simulation and FFB never run
// here. Cheap metadata inspection precedes any 8-MiB eye-frame copy.
#include "stereo_frame_ipc.h"
#include <cstdint>

namespace m2vr::transport {
// x64 must never present an untagged GameStereo image as if it were
// rendered against a known predicted OpenXR eye pair. Older untagged
// producers remain readable at the transport ABI but cannot claim native
// binocular projection. A diagnostic frame is intentionally untagged.
inline constexpr bool SourcePoseMatchesFrame(
    FrameKind kind,std::uint32_t pose_tag,bool exact_pose_in_host_ring) noexcept {
    if(kind==FrameKind::Diagnostic || IsMonoGame(kind))
        return pose_tag==0u;
    return IsStereoGame(kind) &&
           pose_tag!=0u && exact_pose_in_host_ring;
}
enum class FrameUse : std::uint8_t { Missing, ReadNew, HoldCached };
class StereoFrameGate {
public:
    // QPC is monotonic and independent of predicted OpenXR display time.
    // Allow 60 Hz producers to feed 90/120 Hz XR without flickering, but
    // never show a frozen frame indefinitely after the producer disappears.
    static constexpr std::uint64_t kMaxHoldMilliseconds=125;
    // A flat original-game cinema image may be held slightly longer than
    // independently projected world stereo during a slow D3D9 readback.
    // Both paths still expire; an invalid/newer original Present revokes
    // either cached image immediately through producer-health evidence.
    static constexpr std::uint64_t kMaxMonoHoldMilliseconds=250;
    FrameUse Decide(const FrameStamp* stamp,std::uint64_t now,
                    std::uint64_t ticks_per_second) const noexcept {
        if(!ticks_per_second || !now)return FrameUse::Missing;
        const auto cached_fresh = HasRecent(last_qpc_,now,ticks_per_second,kind_);
        const bool cached = last_id_!=0 && cached_fresh;
        if(!stamp || !HasRecent(stamp->timestamp_qpc,now,ticks_per_second,stamp->kind) ||
           stamp->frame_id==0 ||
           !ValidSourceKind(stamp->kind))
            return cached ? FrameUse::HoldCached : FrameUse::Missing;
        // A larger frame ID alone is not a newer physical source sample.
        // Reject equal or reversed QPC stamps, even when still inside the
        // 125/250-ms cache TTL. Hold the previous pair only while it is
        // independently fresh; never copy or relabel rollback pixels.
        if(stamp->frame_id>last_id_)
            return stamp->timestamp_qpc>source_high_qpc_
                ?FrameUse::ReadNew
                :(cached?FrameUse::HoldCached:FrameUse::Missing);
        return stamp->frame_id==last_id_ && cached
            ? FrameUse::HoldCached : FrameUse::Missing;
    }
    bool Accept(const Snapshot& snapshot,std::uint64_t now,
                std::uint64_t ticks_per_second) noexcept {
        if(!HasRecent(snapshot.timestamp_qpc,now,ticks_per_second,snapshot.kind) ||
           snapshot.frame_id==0 || snapshot.frame_id<=last_id_ ||
           snapshot.timestamp_qpc<=source_high_qpc_ ||
           !ValidSourceKind(snapshot.kind) ||
           !ValidEyePayload(snapshot.kind,snapshot.eyes[0].size(),
                            snapshot.eyes[1].size()))return false;
        last_id_=snapshot.frame_id;
        last_qpc_=snapshot.timestamp_qpc;
        source_high_qpc_=snapshot.timestamp_qpc;
        kind_=snapshot.kind;
        return true;
    }
    bool Cached(std::uint64_t now,std::uint64_t ticks_per_second) const noexcept {
        return last_id_ && HasRecent(last_qpc_,now,ticks_per_second,kind_);
    }
    std::uint64_t id()const noexcept{return last_id_;}
    FrameKind kind()const noexcept{return kind_;}
    void Reset()noexcept{
        last_id_=last_qpc_=source_high_qpc_=0;
        kind_=FrameKind::Invalid;
    }
    // XR session STOPPING or lost tracking must revoke cached eye contents
    // while keeping the source's monotonically increasing frame high-water
    // mark. Otherwise a pre-stop IPC image could be accepted a second time
    // immediately after the headset session resumes.
    void ExpireKeepFrameId() noexcept {
        last_qpc_=0u;
        // The source frame ID and its last verified QPC remain monotonic
        // across tracking loss and same-host XR session restarts.
        kind_=FrameKind::Invalid;
    }
private:
    static bool HasRecent(std::uint64_t stamp,std::uint64_t now,
                          std::uint64_t hz,FrameKind kind) noexcept {
        // No potentially overflowing millisecond multiply. For original
        // mono the deadline is hz/4 (250ms); for native stereo hz/8
        // (125ms). At 60Hz source/120Hz display, neither expires normally.
        if(!stamp || !hz || now<stamp)return false;
        const auto allowed=IsMonoGame(kind)?hz/4u:hz/8u;
        return now-stamp<=allowed;
    }
    std::uint64_t last_id_=0;
    std::uint64_t last_qpc_=0;
    std::uint64_t source_high_qpc_=0;
    FrameKind kind_=FrameKind::Invalid;
};
} // namespace m2vr::transport
