#include "../src/probe/daytona_source_pose_commit_gate.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"pose present gate %d\n",__LINE__);return 1;}}while(0)
int main(){
    using namespace m2vr;
    constexpr std::uint64_t now=1000600000ull,hz=10000000ull;
    daytona::OriginalSourcePoseTicket ticket{100u,1000000000ull,1u};
    pose::Frame p{};
    p.sequence=102u;p.published_qpc=1000500000ull;
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
    using daytona::OriginalStereoPoseStillLiveAtPresent;
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
    p.sequence=131u;
    CHECK(OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.sequence=132u;
    CHECK(!OriginalStereoPoseStillLiveAtPresent(ticket,p,now,hz));
    p.sequence=102u;p.published_qpc=999999999ull;
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
