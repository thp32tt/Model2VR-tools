#include "../src/vr/model2_frame_latch.hpp"
#include <cmath>
#include <cstdio>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"latch failure at %d: %s\n",__LINE__,#x);return 1;} } while(0)
int main() {
 using namespace m2vr;
 vr::Model2FramePoseLatch latch;
 pose::Frame p{},snapshot{};
 CHECK(!latch.Read(1,snapshot));
 p.sequence=7;p.predicted_display_time=7000;
 p.view_flags=pose::kFullSixDofTracked;
 for(auto& eye:p.eyes) {
  eye.orientation_xyzw[3]=1.f;
  auto& f=eye.fov_left_right_up_down;
  f[0]=-0.8f;f[1]=0.8f;f[2]=0.8f;f[3]=-0.8f;
 }
 p.eyes[0].position_m[0]=-0.032f;
 p.eyes[1].position_m[0]=0.032f;
 // Initial unknown LOCAL origin must be rejected without consuming game
 // frame or XR sequence. The exact same tracked pair can recover once
 // a real Win64 LOCAL session epoch is supplied.
 CHECK(!latch.Begin(100,p));
 CHECK(!latch.Read(100,snapshot));
 p.reserved=1u;
 CHECK(latch.Begin(100,p));
 CHECK(latch.Read(100,snapshot));
 CHECK(snapshot.sequence==7);
 CHECK(snapshot.eyes[0].position_m[0]<snapshot.eyes[1].position_m[0]);
 p.eyes[0].position_m[0]=123.f; // external changes cannot affect latch
 CHECK(latch.Read(100,snapshot));
 CHECK(snapshot.eyes[0].position_m[0]==-0.032f);
 CHECK(!latch.Read(101,snapshot));
 CHECK(!latch.Begin(100,p)); // never relatch the same game frame
 CHECK(!latch.Read(100,snapshot)); // fail closed, no stale pair
 CHECK(!latch.Begin(101,p)); // pose sequence must advance
 p.eyes[0].position_m[0]=-0.032f; // undo deliberately changed external packet
 p.sequence=8;p.predicted_display_time=8000;
 CHECK(latch.Begin(101,p));
 p.sequence=9;p.predicted_display_time=9000;p.eyes[0].orientation_xyzw[3]=0.f;
 CHECK(!latch.Begin(102,p));
 CHECK(!latch.Read(101,snapshot));
 p.eyes[0].orientation_xyzw[3]=1.f;
 p.eyes[0].fov_left_right_up_down[0]=0.1f;
 CHECK(!latch.Begin(102,p)); // inverted/invalid eye frustum
 p.eyes[0].fov_left_right_up_down[0]=-0.8f;
 p.eyes[1].position_m[1]=INFINITY;
 CHECK(!latch.Begin(102,p));
 p.eyes[1].position_m[1]=0.f;
 CHECK(latch.Begin(102,p));
 latch.Reset();CHECK(!latch.Read(102,snapshot));
 CHECK(latch.Begin(1,p)); // device reset starts a fresh frame epoch
 // The latch is a CPU-world entry point too, not only a Win64 host
 // publication consumer. Reject a forged single eye and revoke any old
 // pose for the failed frame, then recover with the next genuine pair.
 p.sequence=10;p.predicted_display_time=10000;
 p.eyes[1].position_m[0]=p.eyes[0].position_m[0];
 CHECK(!latch.Begin(2,p));
 CHECK(!latch.Read(1,snapshot));
 p.eyes[1].position_m[0]=0.032f;
 CHECK(latch.Begin(2,p));
 p.sequence=11;p.predicted_display_time=11000;
 p.eyes[0].position_m[0]=0.032f;
 p.eyes[1].position_m[0]=-0.032f;
 CHECK(!latch.Begin(3,p));
 p.eyes[0].position_m[0]=-0.032f;
 p.eyes[1].position_m[0]=0.032f;
 CHECK(latch.Begin(3,p));
 p.sequence=12;p.predicted_display_time=12000;
 p.eyes[1].fov_left_right_up_down[0]=-0.21f;
 p.eyes[1].fov_left_right_up_down[1]=0.22f;
 CHECK(!latch.Begin(4,p));
 p.eyes[1].fov_left_right_up_down[0]=-0.8f;
 p.eyes[1].fov_left_right_up_down[1]=0.8f;
 CHECK(latch.Begin(4,p));
 // Adopt a real Win64 LOCAL source generation. Future physical game
 // frames may advance the XR epoch but must never return to an old
 // origin, even when that source packet has a larger pose sequence.
 p.sequence=13;p.predicted_display_time=13000;p.reserved=1u;
 CHECK(latch.Begin(5,p));
 CHECK(latch.ReferenceSpaceEpoch()==1u);
 CHECK(latch.Read(5,snapshot)&&snapshot.reserved==1u);
 p.sequence=14;p.predicted_display_time=14000;p.reserved=0u; // old/legacy source after tracked LOCAL
 CHECK(!latch.Begin(6,p));
 CHECK(!latch.Read(5,snapshot)); // failed refresh revokes prior eyes
 CHECK(latch.ReferenceSpaceEpoch()==1u); // origin high-water stays
 p.sequence=15;p.predicted_display_time=15000;p.reserved=1u;
 CHECK(latch.Begin(6,p)); // reject did not consume next frame/pose
 p.sequence=16;p.predicted_display_time=16000;p.reserved=3u;
 CHECK(latch.Begin(7,p));
 CHECK(latch.ReferenceSpaceEpoch()==3u);
 p.sequence=17;p.predicted_display_time=17000;p.reserved=2u;
 CHECK(!latch.Begin(8,p));
 CHECK(!latch.Read(7,snapshot));
 CHECK(latch.ReferenceSpaceEpoch()==3u);
 p.sequence=18;p.predicted_display_time=18000;p.reserved=4u;
 p.eyes[1].position_m[0]=p.eyes[0].position_m[0];
 CHECK(!latch.Begin(8,p)); // invalid prospective new epoch stays old
 CHECK(latch.ReferenceSpaceEpoch()==3u);
 p.sequence=19;p.predicted_display_time=19000;
 p.eyes[1].position_m[0]=0.032f;
 CHECK(latch.Begin(8,p));
 CHECK(latch.ReferenceSpaceEpoch()==4u);
 CHECK(latch.Read(8,snapshot)&&snapshot.reserved==4u);
 // OpenXR can retain VALID eyes while physical position/orientation
 // tracking is lost. Reject and revoke both eyes without consuming
 // the attempted frame or changing the accepted LOCAL source epoch.
 p.sequence=20u;p.predicted_display_time=20000;
 p.view_flags=pose::kFullSixDofTracked & ~pose::kPositionTracked;
 CHECK(!latch.Begin(9,p));
 CHECK(!latch.Read(8,snapshot));
 CHECK(latch.ReferenceSpaceEpoch()==4u);
 p.view_flags=pose::kFullSixDofTracked;
 CHECK(latch.Begin(9,p));
 p.sequence=21u;p.predicted_display_time=21000;
 p.view_flags=pose::kFullSixDofTracked & ~pose::kOrientationTracked;
 CHECK(!latch.Begin(10,p));
 CHECK(!latch.Read(9,snapshot));
 p.view_flags=pose::kFullSixDofTracked;
 CHECK(latch.Begin(10,p));
 // The independent native x86 CPU frame latch must reject a newer
 // source sequence with the same, reversed or missing predicted XrTime.
 // Failure revokes old eyes but does NOT consume frame/pose high-water.
 p.sequence=22u;p.predicted_display_time=21000;
 CHECK(!latch.Begin(11,p));
 CHECK(!latch.Read(10,snapshot));
 p.predicted_display_time=22000;
 CHECK(latch.Begin(11,p));
 p.sequence=23u;p.predicted_display_time=0;
 CHECK(!latch.Begin(12,p));
 p.predicted_display_time=23000;
 CHECK(latch.Begin(12,p));
 p.sequence=24u;p.predicted_display_time=22999;
 CHECK(!latch.Begin(13,p));
 p.predicted_display_time=24000;
 CHECK(latch.Begin(13,p));
 // Even a forged future LOCAL generation cannot excuse an old time.
 p.sequence=25u;p.reserved=5u;p.predicted_display_time=24000;
 CHECK(!latch.Begin(14,p));
 CHECK(latch.ReferenceSpaceEpoch()==4u);
 p.predicted_display_time=25000;
 CHECK(latch.Begin(14,p));
 CHECK(latch.ReferenceSpaceEpoch()==5u);
 // Only an explicit game/device reset may intentionally start an
 // independent source-consumer lifetime with a lower epoch.
 latch.Reset();
 CHECK(latch.ReferenceSpaceEpoch()==0u);
 p.sequence=1;p.predicted_display_time=1000;p.reserved=1u;
 CHECK(latch.Begin(1,p));
 CHECK(latch.ReferenceSpaceEpoch()==1u);
 return 0;
}
