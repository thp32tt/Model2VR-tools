#pragma once
// Original HMD pose used to render a GameStereo pair must still be fresh
// at the exact original D3D9 Present. Newer poses do not refresh old images.
// Read-only source evidence: does not authenticate the Daytona race world.
#include "daytona_eye_pose_gate.hpp"
#include <cstdint>
namespace m2vr::daytona {
struct OriginalSourcePoseTicket {
    std::uint64_t sequence=0;
    std::uint64_t published_qpc=0;
};
inline bool OriginalStereoPoseStillLiveAtPresent(
    OriginalSourcePoseTicket ticket,const pose::Frame& latest,
    std::uint64_t now,std::uint64_t hz) noexcept {
    if(!ticket.sequence || ticket.sequence>0xffffffffull ||
       !ticket.published_qpc || hz<8u ||
       now<ticket.published_qpc ||
       now-ticket.published_qpc>hz/8u ||
       latest.sequence<ticket.sequence ||
       latest.sequence-ticket.sequence>32u ||
       latest.published_qpc<ticket.published_qpc ||
       latest.published_qpc>now)return false;
    return AdmitTrackedWorldEyePose(latest,now,hz).valid &&
           CanTagGameStereoPose(latest.sequence);
}
} // namespace m2vr::daytona
