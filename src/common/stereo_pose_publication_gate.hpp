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
template<class Writer,class Pair,std::size_t Slots>
bool PublishTrackedStereoPose(
    Writer& writer,transport::StereoPoseHistory<Pair,Slots>& history,
    const Frame& packet,const Pair& actual_views) noexcept {
    const auto revoke=[&]() noexcept {
        if(writer.Valid())writer.Invalidate();
        history.Clear();
    };
    if(!CheckStereoRigGeometry(packet).valid() ||
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
    return true;
}
} // namespace m2vr::pose
