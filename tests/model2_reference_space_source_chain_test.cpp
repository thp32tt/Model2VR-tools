// Real source boundary contract, tested headlessly on original x86 + XR x64.
// The original game, physical HMD camera and ROM are NEVER simulated as proof.
#include "../src/common/model2_reference_space_epoch.hpp"
#include "../src/common/stereo_pose_publication_gate.hpp"
#include "../src/probe/daytona_source_pose_commit_gate.hpp"
#include "../src/vr/model2_head_recenter.hpp"
#include "../src/vr/model2_frame_latch.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define REQUIRE(v) do {if(!(v)){std::fprintf(stderr,"LOCAL XR source chain %d: %s\n",__LINE__,#v);std::exit(1);}} while(0)
using Views=std::array<unsigned,2>;
using Ring=m2vr::transport::StereoPoseHistory<Views,4>;
struct FakeHostWriter {
    Ring* ring=nullptr;
    bool active=true;
    m2vr::pose::Frame published{};
    bool Valid()const noexcept{return active;}
    bool Invalidate()noexcept{published={};return true;}
    bool Publish(const m2vr::pose::Frame& frame)noexcept {
        if(!ring || !ring->Find(static_cast<std::uint32_t>(frame.sequence)))
            return false;
        published=frame;
        return true;
    }
};
static m2vr::pose::Frame Packet(
    std::uint64_t sequence,std::uint32_t epoch,float head_x){
    m2vr::pose::Frame frame{};
    frame.sequence=sequence;
    frame.reserved=epoch;
    frame.published_qpc=1000000000ull+sequence;
    frame.view_flags=m2vr::pose::kFullSixDofTracked;
    for(std::size_t i=0;i<2;++i){
        auto& eye=frame.eyes[i];
        eye.orientation_xyzw[3]=1.f;
        eye.position_m[0]=head_x+(i?0.032f:-0.032f);
        eye.fov_left_right_up_down[0]=-0.8f;
        eye.fov_left_right_up_down[1]=0.8f;
        eye.fov_left_right_up_down[2]=0.8f;
        eye.fov_left_right_up_down[3]=-0.8f;
    }
    return frame;
}
static bool Near(float a,float b) {return std::abs(a-b)<0.0005f;}
int main(){
    using namespace m2vr;
    pose::ReferenceSpaceEpochGate epoch;
    REQUIRE(epoch.BeginSession() && epoch.Epoch()==1);
    Ring history;
    FakeHostWriter writer{&history};
    vr::Model2HeadRecenter cpu;
    vr::Model2FramePoseLatch game_pose;
    const auto earlier=Packet(10,epoch.Epoch(),2.f);
    REQUIRE(pose::PublishTrackedStereoPose(
        writer,history,earlier,Views{101u,102u}));
    REQUIRE(writer.published.sequence==earlier.sequence);
    REQUIRE(history.Find(10u));
    REQUIRE(game_pose.Begin(100u,writer.published));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==1u);
    pose::Frame cpu_eyes{};
    REQUIRE(cpu.Rebase(writer.published,cpu_eyes));
    REQUIRE(cpu.ReferenceSpaceEpoch()==1u);
    REQUIRE(Near(cpu_eyes.eyes[0].position_m[0],-0.032f));
    // The OpenXR source can retain VALID while 6DoF tracking is lost.
    // Do not admit it even when the stereo rig geometry remains plausible.
    auto temporarily_untracked=Packet(11u,epoch.Epoch(),9.f);
    temporarily_untracked.view_flags &= ~pose::kPositionTracked;
    REQUIRE(!game_pose.Begin(101u,temporarily_untracked));
    REQUIRE(!cpu.Rebase(temporarily_untracked,cpu_eyes));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==1u);
    REQUIRE(cpu.ReferenceSpaceEpoch()==1u);
    REQUIRE(!game_pose.Read(100u,temporarily_untracked));
    const daytona::OriginalSourcePoseTicket source_ticket{
        earlier.sequence,earlier.published_qpc,earlier.reserved};
    constexpr std::uint64_t hz=10000000ull;
    constexpr std::uint64_t now=1000000300ull;
    REQUIRE(daytona::OriginalStereoPoseStillLiveAtPresent(
        source_ticket,earlier,now,hz));
    // OpenXR event arrives BEFORE effective changeTime: old ORIGINAL
    // game pixels and cached exact-pose history are revoked immediately.
    REQUIRE(epoch.NotifyLocalChange(200));
    writer.Invalidate();
    history.Clear();
    REQUIRE(history.Find(10u)==nullptr);
    REQUIRE(epoch.OnPredictedFrame(199)==
            pose::ReferenceSpaceFrame::SuppressPending);
    REQUIRE(epoch.OnPredictedFrame(200)==
            pose::ReferenceSpaceFrame::SuppressTransition);
    REQUIRE(epoch.Epoch()==2);
    REQUIRE(epoch.OnPredictedFrame(201)==pose::ReferenceSpaceFrame::Ready);
    // New-LOCAL tracked eyes may be perfectly valid while physically
    // unrelated to the old original game Draw's reference-space ticket.
    const auto new_local=Packet(11,epoch.Epoch(),44.f);
    REQUIRE(pose::PublishTrackedStereoPose(
        writer,history,new_local,Views{201u,202u}));
    REQUIRE(!daytona::OriginalStereoPoseStillLiveAtPresent(
        source_ticket,new_local,now,hz));
    REQUIRE(cpu.Rebase(new_local,cpu_eyes));
    REQUIRE(cpu.ReferenceSpaceEpoch()==2u);
    REQUIRE(game_pose.Begin(101u,new_local));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==2u);
    // Direct CPU-world recenter is an independent native consumer; a
    // delayed tracked packet from this SAME LOCAL generation cannot
    // rewind the already accepted headset-camera sequence.
    const auto delayed_same_local=Packet(10u,epoch.Epoch(),44.f);
    REQUIRE(!cpu.Rebase(delayed_same_local,cpu_eyes));
    REQUIRE(cpu.ReferenceSpaceEpoch()==2u);
    REQUIRE(cpu.LastPoseSequence()==11u);
    REQUIRE(cpu_eyes.sequence==11u);
    // A delayed old-LOCAL source with a HIGHER pose sequence remains
    // unsafe even though the eyes are geometrically plausible.
    const auto late_old=Packet(99u,1u,77.f);
    REQUIRE(!game_pose.Begin(102u,late_old));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==2u);
    pose::Frame rejected_latch{};
    REQUIRE(!game_pose.Read(101u,rejected_latch));
    const auto late_legacy=Packet(100u,0u,77.f);
    REQUIRE(!game_pose.Begin(102u,late_legacy));
    REQUIRE(!cpu.Rebase(late_legacy,cpu_eyes));
    REQUIRE(!daytona::OriginalStereoPoseStillLiveAtPresent(
        {late_legacy.sequence,late_legacy.published_qpc,0u},
        late_legacy,now,hz));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==2u);
    REQUIRE(Near(cpu_eyes.eyes[0].position_m[0],-0.032f));
    REQUIRE(Near(cpu_eyes.eyes[1].position_m[0],0.032f));
    REQUIRE(!cpu.Rebase(earlier,cpu_eyes));
    REQUIRE(history.Find(10u)==nullptr && history.Find(11u)!=nullptr);
    // Events lost can hide an unknown future LOCAL recenter: do not
    // guess a new origin, stop BOTH eyes until actual session restart.
    REQUIRE(epoch.NotifyEventsLost());
    writer.Invalidate();
    history.Clear();
    REQUIRE(epoch.OnPredictedFrame(202)==pose::ReferenceSpaceFrame::Exhausted);
    REQUIRE(epoch.OnPredictedFrame(9999)==pose::ReferenceSpaceFrame::Exhausted);
    REQUIRE(epoch.BeginSession() && epoch.Epoch()==3);
    const auto restart=Packet(12,epoch.Epoch(),-5.f);
    REQUIRE(pose::PublishTrackedStereoPose(writer,history,restart,Views{3u,4u}));
    REQUIRE(!daytona::OriginalStereoPoseStillLiveAtPresent(
        {11u,new_local.published_qpc,new_local.reserved},restart,now,hz));
    REQUIRE(cpu.Rebase(restart,cpu_eyes));
    REQUIRE(cpu.ReferenceSpaceEpoch()==3u);
    REQUIRE(game_pose.Begin(102u,restart));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==3u);
    REQUIRE(Near(cpu_eyes.eyes[0].position_m[0],-0.032f));
    // A second LOCAL notification without an applicable transition time
    // cannot borrow the previous known pending time or the last accepted
    // original x86 CPU-world pose. Only real session restart unblocks it.
    REQUIRE(epoch.NotifyLocalChange(5000));
    writer.Invalidate();
    history.Clear();
    REQUIRE(epoch.NotifyLocalChange(0) && epoch.ChangeTimeUnknown());
    REQUIRE(!epoch.Pending());
    REQUIRE(epoch.OnPredictedFrame(5000)==pose::ReferenceSpaceFrame::Exhausted);
    REQUIRE(epoch.OnPredictedFrame(999999)==pose::ReferenceSpaceFrame::Exhausted);
    REQUIRE(!epoch.NotifyLocalChange(6000));
    REQUIRE(writer.published.sequence==0 && !history.Find(12u));
    REQUIRE(epoch.BeginSession() && epoch.Epoch()==4u);
    REQUIRE(!epoch.ChangeTimeUnknown());
    REQUIRE(epoch.OnPredictedFrame(6001)==pose::ReferenceSpaceFrame::Ready);
    const auto final_local=Packet(13,epoch.Epoch(),-20.f);
    REQUIRE(pose::PublishTrackedStereoPose(
        writer,history,final_local,Views{7u,8u}));
    REQUIRE(!daytona::OriginalStereoPoseStillLiveAtPresent(
        {restart.sequence,restart.published_qpc,restart.reserved},
        final_local,now,hz));
    REQUIRE(cpu.Rebase(final_local,cpu_eyes));
    REQUIRE(cpu.ReferenceSpaceEpoch()==4u);
    REQUIRE(game_pose.Begin(103u,final_local));
    REQUIRE(game_pose.ReferenceSpaceEpoch()==4u);
    REQUIRE(Near(cpu_eyes.eyes[0].position_m[0],-0.032f));
    // A valid new LOCAL pose does NOT make an older predicted frame safe.
    // The actual Win64 draw_frame branch revokes IPC and cached images on
    // Exhausted; mirror that source transaction with synthetic records.
    const auto bad_time=epoch.OnPredictedFrame(6000);
    REQUIRE(bad_time==pose::ReferenceSpaceFrame::Exhausted);
    if(bad_time==pose::ReferenceSpaceFrame::Exhausted){
        REQUIRE(writer.Invalidate());
        history.Clear();
    }
    REQUIRE(epoch.TimeDiscontinuity());
    REQUIRE(writer.published.sequence==0u);
    REQUIRE(history.Find(13u)==nullptr);
    REQUIRE(epoch.OnPredictedFrame(7000)==pose::ReferenceSpaceFrame::Exhausted);
    REQUIRE(epoch.BeginSession() && epoch.Epoch()==5u);
    REQUIRE(epoch.OnPredictedFrame(1)==pose::ReferenceSpaceFrame::Ready);
    return 0;
}
