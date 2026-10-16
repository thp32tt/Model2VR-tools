#include "../src/vr/model2_frame_latch.hpp"
#include <cmath>
#include <cstdio>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"latch failure at %d: %s\n",__LINE__,#x);return 1;} } while(0)
int main() {
 using namespace m2vr;
 vr::Model2FramePoseLatch latch;
 pose::Frame p{},snapshot{};
 CHECK(!latch.Read(1,snapshot));
 p.sequence=7;
 p.view_flags=pose::kOrientationValid|pose::kPositionValid;
 for(auto& eye:p.eyes) {
  eye.orientation_xyzw[3]=1.f;
  auto& f=eye.fov_left_right_up_down;
  f[0]=-0.8f;f[1]=0.8f;f[2]=0.8f;f[3]=-0.8f;
 }
 p.eyes[0].position_m[0]=-0.032f;
 p.eyes[1].position_m[0]=0.032f;
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
 p.sequence=8;
 CHECK(latch.Begin(101,p));
 p.sequence=9;p.eyes[0].orientation_xyzw[3]=0.f;
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
 return 0;
}
