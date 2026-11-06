#pragma once
// A real OpenXR LOCAL reference-space recenter invalidates the coordinate
// origin used by both independently rendered eyes. Keep the exact effective
// changeTime across future predicted XR frames and fail closed at transition.
// Uses existing stereo_pose_ipc::Frame.reserved; no ABI layout change.
#include <cstdint>
#include <limits>
namespace m2vr::pose {
enum class ReferenceSpaceFrame : std::uint8_t {
    Ready, SuppressPending, SuppressTransition, Exhausted
};
class ReferenceSpaceEpochGate final {
public:
    bool BeginSession() noexcept {
        pending_=false;
        change_time_=0;
        events_lost_=false;
        unknown_change_time_=false;
        return Advance();
    }
    bool NotifyLocalChange(std::int64_t change_time) noexcept {
        if(!epoch_ || exhausted_ || events_lost_ || unknown_change_time_)return false;
        // An absent/nonpositive changeTime does NOT authorize guessing the
        // first post-recenter xrLocateViews frame. Its true transition could
        // still be in the future, including after another queued event.
        // Keep both original x86 eyes quarantined until an XR session restart.
        if(change_time<=0){
            pending_=false;
            change_time_=0;
            unknown_change_time_=true;
            return true;
        }
        if(!pending_)change_time_=change_time;
        else if(change_time>change_time_)change_time_=change_time;
        pending_=true;
        return true;
    }
    // XR_TYPE_EVENT_DATA_EVENTS_LOST gives no LOCAL changeTime. A
    // lost event could refer to a FUTURE recenter, so reanchoring NOW
    // would merely accept the wrong origin later. Quarantine every frame
    // until a real XR session restart starts a fresh trusted generation.
    bool NotifyEventsLost() noexcept {
        if(!epoch_ || exhausted_)return false;
        pending_=false;
        change_time_=0;
        events_lost_=true;
        return true;
    }
    ReferenceSpaceFrame OnPredictedFrame(std::int64_t display_time) noexcept {
        if(!epoch_ || exhausted_ || events_lost_ || unknown_change_time_)
            return ReferenceSpaceFrame::Exhausted;
        if(!pending_)return ReferenceSpaceFrame::Ready;
        if(display_time<=0 || (change_time_>0 &&
                               display_time<change_time_))
            return ReferenceSpaceFrame::SuppressPending;
        if(!Advance())return ReferenceSpaceFrame::Exhausted;
        pending_=false;
        change_time_=0;
        // No old original-game frame is permitted on the boundary tick.
        return ReferenceSpaceFrame::SuppressTransition;
    }
    std::uint32_t Epoch()const noexcept{return epoch_;}
    bool Pending()const noexcept{return pending_;}
    bool EventsLost()const noexcept{return events_lost_;}
    bool ChangeTimeUnknown()const noexcept{return unknown_change_time_;}
private:
    bool Advance() noexcept {
        if(exhausted_ || epoch_==std::numeric_limits<std::uint32_t>::max()){
            exhausted_=true;
            return false;
        }
        ++epoch_;
        return true;
    }
    std::uint32_t epoch_=0;
    std::int64_t change_time_=0;
    bool pending_=false,events_lost_=false,unknown_change_time_=false,
         exhausted_=false;
};
} // namespace m2vr::pose
