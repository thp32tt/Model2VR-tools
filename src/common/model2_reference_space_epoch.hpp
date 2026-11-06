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
        return Advance();
    }
    bool NotifyLocalChange(std::int64_t change_time) noexcept {
        if(!epoch_ || exhausted_)return false;
        if(!pending_)change_time_=change_time>0?change_time:0;
        else if(change_time>change_time_)change_time_=change_time;
        pending_=true;
        return true;
    }
    // XR_TYPE_EVENT_DATA_EVENTS_LOST has no reliable changeTime.
    // It may conceal an already-effective recenter, so ignore any older
    // future threshold and force a fresh LOCAL origin on the next frame.
    bool NotifyEventsLost() noexcept {
        if(!epoch_ || exhausted_)return false;
        pending_=true;
        change_time_=0;
        return true;
    }
    ReferenceSpaceFrame OnPredictedFrame(std::int64_t display_time) noexcept {
        if(!epoch_ || exhausted_)return ReferenceSpaceFrame::Exhausted;
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
    bool pending_=false,exhausted_=false;
};
} // namespace m2vr::pose
