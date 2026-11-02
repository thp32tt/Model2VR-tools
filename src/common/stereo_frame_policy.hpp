#pragma once
// Source-facing stereo host admission gate. Game simulation and FFB never run
// here. Cheap metadata inspection precedes any 8-MiB eye-frame copy.
#include "stereo_frame_ipc.h"
#include <cstdint>

namespace m2vr::transport {
enum class FrameUse : std::uint8_t { Missing, ReadNew, HoldCached };
class StereoFrameGate {
public:
    // QPC is monotonic and independent of predicted OpenXR display time.
    // Allow 60 Hz producers to feed 90/120 Hz XR without flickering, but
    // never show a frozen frame indefinitely after the producer disappears.
    static constexpr std::uint64_t kMaxHoldMilliseconds=125;
    FrameUse Decide(const FrameStamp* stamp,std::uint64_t now,
                    std::uint64_t ticks_per_second) const noexcept {
        if(!ticks_per_second || !now)return FrameUse::Missing;
        const auto cached_fresh = HasRecent(last_qpc_,now,ticks_per_second);
        const bool cached = last_id_!=0 && cached_fresh;
        if(!stamp || !HasRecent(stamp->timestamp_qpc,now,ticks_per_second) ||
           stamp->frame_id==0 ||
           (stamp->kind!=FrameKind::GameStereo &&
            stamp->kind!=FrameKind::Diagnostic))
            return cached ? FrameUse::HoldCached : FrameUse::Missing;
        if(stamp->frame_id>last_id_)return FrameUse::ReadNew;
        return stamp->frame_id==last_id_ && cached
            ? FrameUse::HoldCached : FrameUse::Missing;
    }
    bool Accept(const Snapshot& snapshot,std::uint64_t now,
                std::uint64_t ticks_per_second) noexcept {
        if(!HasRecent(snapshot.timestamp_qpc,now,ticks_per_second) ||
           snapshot.frame_id==0 || snapshot.frame_id<=last_id_ ||
           (snapshot.kind!=FrameKind::GameStereo &&
            snapshot.kind!=FrameKind::Diagnostic) ||
           snapshot.eyes[0].size()!=kBytesPerEye ||
           snapshot.eyes[1].size()!=kBytesPerEye)return false;
        last_id_=snapshot.frame_id;
        last_qpc_=snapshot.timestamp_qpc;
        kind_=snapshot.kind;
        return true;
    }
    bool Cached(std::uint64_t now,std::uint64_t ticks_per_second) const noexcept {
        return last_id_ && HasRecent(last_qpc_,now,ticks_per_second);
    }
    std::uint64_t id()const noexcept{return last_id_;}
    FrameKind kind()const noexcept{return kind_;}
    void Reset()noexcept{last_id_=last_qpc_=0;kind_=FrameKind::Invalid;}
private:
    static bool HasRecent(std::uint64_t stamp,std::uint64_t now,
                          std::uint64_t hz) noexcept {
        // Avoid multiplication overflow: 125ms = hz/8. The 125ms hold
        // must work at the normal 10MHz QPC frequency and at 1GHz.
        if(!stamp || !hz || now<stamp)return false;
        const auto allowed=hz/8;
        return now-stamp<=allowed;
    }
    std::uint64_t last_id_=0;
    std::uint64_t last_qpc_=0;
    FrameKind kind_=FrameKind::Invalid;
};
} // namespace m2vr::transport
