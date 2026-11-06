#include "../src/common/stereo_pose_history.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"pose history line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    m2vr::transport::StereoPoseHistory<std::array<float,2>,32> ring;
    CHECK(ring.Find(0)==nullptr);
    CHECK(ring.Find(1)==nullptr);
    const std::array<float,2> first{-0.032f,0.032f};
    const std::array<float,2> next{-0.04f,0.04f};
    CHECK(!ring.Record(0,first));
    CHECK(!ring.Record(std::uint64_t{0xffffffffu}+1u,first));
    CHECK(ring.Record(1,first));
    CHECK(ring.Find(1) && *ring.Find(1)==first);
    CHECK(ring.Find(2)==nullptr);
    CHECK(ring.Record(2,next));
    CHECK(ring.Find(2) && *ring.Find(2)==next);
    CHECK(ring.Record(33,next));
    CHECK(ring.Find(1)==nullptr); // same ring slot; stale pair not reused
    CHECK(ring.Find(33) && *ring.Find(33)==next);
    CHECK(ring.Find(2) && *ring.Find(2)==next);
    CHECK(!ring.Record(std::numeric_limits<std::uint64_t>::max(),first));
    CHECK(ring.Find(33) && *ring.Find(33)==next); // rejected input atomic
    ring.Clear();
    CHECK(ring.Find(2)==nullptr && ring.Find(33)==nullptr);
    CHECK(!ring.Record(33,first)); // revoked tag is never reusable
    CHECK(!ring.Record(1,first));
    CHECK(ring.Record(34,first));
    CHECK(ring.Find(34) && *ring.Find(34)==first);
    return 0;
}
