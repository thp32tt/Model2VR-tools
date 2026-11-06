#include "../src/probe/daytona_source_pose_commit_gate.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"pose present gate %d\n",__LINE__);return 1;}}while(0)
int main(){
    using namespace m2vr;
    constexpr std::uint64_t now=1000600000ull,hz=10000000ull;
    daytona::OriginalSourcePoseTicket ticket{};
    pose::Frame p{};
    p.sequence=102u;p.published_qpc=1000500000ull;
    p.predicted_display_time=102000;
    p.reserved=1u;
    p.view_flags=pose::kFullSixDofTracked;
    for(auto& e:p.eyes){
        e.orientation_xyzw[3]=1.f;
        e.fov_left_right_up_down[0]=-0.8f;
        e.fov_left_right_up_down[1]=0.8f;
        e.fov_left_right_up_down[2]=0.8f;
        e.fov_left_right_up_down[3]=-0.8f;
    }
    p.eyes[0].position_m[0]=-0.032f;
    p.eyes[1].position_m[0]=0.032f;
    auto original_draw=p;
    original_draw.sequence=100u;
    original_draw.published_qpc=1000000000ull;
    original_draw.predicted_display_time=100000;
    ticket=daytona::OriginalSourcePoseTicket::FromDraw(original_draw);
    using daytona::OriginalStereoPoseStillLiveAtPresent;
    auto missing_eye_capture=ticket;
    missing_eye_capture.draw_eyes_captured=false;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(missing_eye_capture,p,now,hz));
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    // Newly tracked Present cannot authenticate VALID-only original Draw.
    auto untracked_draw=ticket;
    untracked_draw.draw_view_flags=pose::kOrientationValid|pose::kPositionValid;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(untracked_draw,p,now,hz));
    untracked_draw.draw_view_flags=pose::kFullSixDofTracked&~pose::kPositionTracked;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(untracked_draw,p,now,hz));
    untracked_draw.draw_view_flags=pose::kFullSixDofTracked&~pose::kOrientationTracked;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(untracked_draw,p,now,hz));
    untracked_draw.draw_view_flags=0u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(untracked_draw,p,now,hz));
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    // Draw-side predicted XrTime must be named, never inferred from QPC.
    auto missing_draw_time=ticket;
    missing_draw_time.predicted_display_time=0;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(missing_draw_time,p,now,hz));
    missing_draw_time.predicted_display_time=-1;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(missing_draw_time,p,now,hz));
    // A newer pose sequence cannot re-label older/equal XR geometry.
    p.predicted_display_time=100000;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.predicted_display_time=99999;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.predicted_display_time=102000;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    // Same source sequence must keep the exact sampled display time.
    p.sequence=100u;p.predicted_display_time=100001;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.predicted_display_time=100000;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    // A same-tag substitute eye image must fail, even when both views
    // are still individually plausible tracked binocular cameras.
    p.eyes[0].position_m[1]=0.001f;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.eyes[0].position_m[1]=0.f;
    p.eyes[1].fov_left_right_up_down[1]=0.79f;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.eyes[1].fov_left_right_up_down[1]=0.8f;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.sequence=102u;p.predicted_display_time=102000;
    // Physical original D3D9 Present must reject a direct tracked-world
    // pose with a missing/reversed predicted XrTime, not only bad QPC age.
    p.predicted_display_time=0;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.predicted_display_time=-1;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.predicted_display_time=102000;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    // No provenance can be inferred from matching zero/legacy origins.
    auto no_source_ticket=ticket;
    no_source_ticket.reference_space_epoch=0u;
    auto no_source_pose=p;no_source_pose.reserved=0u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(
        no_source_ticket,no_source_pose,now,hz));
    CHECK(!OriginalStereoPoseStillLiveAtPresent(
        no_source_ticket,p,now,hz));
    CHECK(!OriginalStereoPoseStillLiveAtPresent(
        ticket,no_source_pose,now,hz));
    // A tracked and fresh HMD packet from another LOCAL origin cannot
    // certify an earlier genuine original D3D9 Draw at physical Present.
    ticket.reference_space_epoch=1u;
    p.reserved=1u;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.reserved=2u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.reserved=0u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.reserved=1u;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,1001250000ull,hz));
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,1001250001ull,hz));
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,999999999ull,hz));
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,0u));
    CHECK(!OriginalStereoPoseStillLiveAtPresent({0u,ticket.published_qpc},p,now,hz));
    CHECK(!OriginalStereoPoseStillLiveAtPresent({0x100000000ull,ticket.published_qpc},p,now,hz));
    p.sequence=99u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.sequence=131u;p.predicted_display_time=131000;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.sequence=132u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.sequence=102u;p.predicted_display_time=102000;
    p.published_qpc=999999999ull;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.published_qpc=now+1u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.published_qpc=1000500000ull;
    p.view_flags&=~pose::kPositionTracked;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.view_flags=pose::kFullSixDofTracked;
    p.eyes[1].position_m[0]=p.eyes[0].position_m[0];
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    return 0;
}
