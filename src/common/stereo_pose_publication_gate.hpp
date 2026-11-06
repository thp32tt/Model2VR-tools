#pragma once
// An original Win32 game can read the host's tracked XR pose IPC the moment
// it is published. Record the corresponding real xrLocateViews views in the
// Win64 tag history FIRST; never expose a tag that has no compositor eyes.
// No synthetic camera, game Draw or World3D authorization is performed here.
#include "stereo_eye_rig_geometry.hpp"
#include "stereo_pose_history.hpp"
#include <cstdint>
#include <limits>
namespace m2vr::pose {
// Keep the last published physical OpenXR time and LOCAL generation.
// Invalidate/Clear revokes visible eyes, but must never rewind this order.
// Only a genuine xrBeginSession may begin a fresh runtime XrTime baseline.
class StereoPosePublicationOrder final {
public:
    bool Allows(const Frame& packet) const noexcept {
        return packet.reserved!=0u && packet.predicted_display_time>0 &&
               (!last_predicted_time_ ||
                packet.predicted_display_time>last_predicted_time_) &&
               (!last_local_epoch_ || packet.reserved>=last_local_epoch_);
    }
    void Published(const Frame& packet) noexcept {
        last_predicted_time_=packet.predicted_display_time;
        last_local_epoch_=packet.reserved;
    }
    void BeginSession() noexcept {
        last_predicted_time_=0;
        last_local_epoch_=0;
    }
private:
    std::int64_t last_predicted_time_=0;
    std::uint32_t last_local_epoch_=0;
};
template<class Writer,class Pair,std::size_t Slots>
bool PublishTrackedStereoPose(
    Writer& writer,transport::StereoPoseHistory<Pair,Slots>& history,
    StereoPosePublicationOrder& order,
    const Frame& packet,const Pair& actual_views) noexcept {
    const auto revoke=[&]() noexcept {
        if(writer.Valid())writer.Invalidate();
        history.Clear();
    };
    // The host's OpenXR frame gate normally provides these conditions,
    // but this is the LAST independent boundary before original Win32
    // can read the predicted HMD source via shared memory. Neither a
    // VALID-only last-known pose nor a missing/reversed XrTime may enter
    // the host eye history and its original game pose IPC together.
    if((packet.view_flags & kFullSixDofTracked)!=kFullSixDofTracked ||
       packet.predicted_display_time<=0 ||
       !order.Allows(packet) ||
       !CheckStereoRigGeometry(packet).valid() ||
       !packet.reserved ||  // a new Win64 host MUST name its LOCAL epoch
       packet.sequence>std::numeric_limits<std::uint32_t>::max() ||
       !writer.Valid()){
        revoke();
        return false;
    }
    // This ordering is essential: Win32 may observe the new packet on a
    // different CPU immediately after the seqlock's final even generation.
    if(!history.Record(packet.sequence,actual_views)){
        revoke();
        return false;
    }
    if(!writer.Publish(packet)){
        revoke();
        return false;
    }
    // Failed publication does not poison a valid next source time.
    order.Published(packet);
    return true;
}
} // namespace m2vr::pose
