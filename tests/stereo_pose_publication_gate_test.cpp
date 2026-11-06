#include "../src/common/stereo_pose_publication_gate.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#define CHECK(v) do {if(!(v)){std::fprintf(stderr,"pose transaction line %d: %s\\n",__LINE__,#v);std::exit(1);}}while(0)
using Pair=std::array<int,2>;
using Ring=m2vr::transport::StereoPoseHistory<Pair,4>;
using m2vr::pose::Frame;
struct Writer {
    Ring* ring=nullptr;
    bool available=true,fail=false,invalidated=false;
    std::uint64_t visible=0;
    bool Valid()const noexcept{return available;}
    bool Invalidate()noexcept{visible=0;invalidated=true;return true;}
    bool Publish(const Frame& f)noexcept{
        // This is the exact point at which Win32 can see the new pose.
        if(fail || !ring->Find(static_cast<std::uint32_t>(f.sequence)))
            return false;
        visible=f.sequence;
        return true;
    }
};
Frame Valid(std::uint64_t id){
    Frame f{};
    f.sequence=id;
    f.view_flags=m2vr::pose::kFullSixDofTracked;
    for(auto& eye:f.eyes){
        eye.orientation_xyzw[3]=1.f;
        eye.fov_left_right_up_down[0]=-0.8f;
        eye.fov_left_right_up_down[1]=0.8f;
        eye.fov_left_right_up_down[2]=0.8f;
        eye.fov_left_right_up_down[3]=-0.8f;
    }
    f.eyes[0].position_m[0]=-0.032f;
    f.eyes[1].position_m[0]=0.032f;
    return f;
}
int main(){
    using m2vr::pose::PublishTrackedStereoPose;
    Ring ring;
    Writer writer{&ring};
    CHECK(PublishTrackedStereoPose(writer,ring,Valid(12),Pair{1,2}));
    CHECK(writer.visible==12 && ring.Find(12) && *ring.Find(12)==Pair({1,2}));
    CHECK(!PublishTrackedStereoPose(writer,ring,Valid(12),Pair{3,4}));
    CHECK(writer.invalidated && !ring.Find(12));
    CHECK(PublishTrackedStereoPose(writer,ring,Valid(13),Pair{3,4}));
    writer.fail=true;writer.invalidated=false;
    CHECK(!PublishTrackedStereoPose(writer,ring,Valid(14),Pair{5,6}));
    CHECK(writer.invalidated && !ring.Find(13) && !ring.Find(14));
    writer.fail=false;
    CHECK(PublishTrackedStereoPose(writer,ring,Valid(15),Pair{5,6}));
    auto mono=Valid(16);mono.eyes[1].position_m[0]=-0.032f;
    CHECK(!PublishTrackedStereoPose(writer,ring,mono,Pair{7,8}));
    CHECK(!ring.Find(15) && writer.visible==0);
    CHECK(!PublishTrackedStereoPose(writer,ring,
        Valid(std::uint64_t{std::numeric_limits<std::uint32_t>::max()}+1u),
        Pair{7,8}));
    writer.available=false;
    CHECK(!PublishTrackedStereoPose(writer,ring,Valid(17),Pair{7,8}));
    CHECK(!ring.Find(17));
    writer.available=true;
    CHECK(PublishTrackedStereoPose(writer,ring,Valid(18),Pair{7,8}));
    ring.Clear();
    CHECK(!ring.Record(18,Pair{9,10}));
    CHECK(ring.Record(19,Pair{9,10}));
    return 0;
}
