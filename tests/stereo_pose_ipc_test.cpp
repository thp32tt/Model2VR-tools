#include "../src/common/stereo_pose_ipc.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <thread>
#define REQUIRE(expr) do {if(!(expr)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#expr);std::exit(1);}}while(0)
int main(){
    using namespace m2vr::pose;
    Link host,emulator;
    const auto key=GetCurrentProcessId();
    REQUIRE(host.Create(key));
    REQUIRE(!emulator.Create(key));
    REQUIRE(emulator.Open(key));
    Frame got{};
    REQUIRE(!emulator.Read(got));
    Frame frame{};
    frame.sequence=1;
    frame.predicted_display_time=8123456;
    frame.view_flags=kOrientationValid|kPositionValid;
    frame.eyes[0].position_m[0]=-0.032f;
    frame.eyes[1].position_m[0]=0.032f;
    frame.eyes[0].fov_left_right_up_down[0]=-0.79f;
    frame.eyes[1].fov_left_right_up_down[1]=0.8f;
    REQUIRE(host.Publish(frame));
    REQUIRE(!host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==1 && got.predicted_display_time==8123456);
    REQUIRE(!emulator.ReadTracked(got)); // VALID is not tracked
    REQUIRE(got.eyes[0].position_m[0]<0 && got.eyes[1].position_m[0]>0);
    REQUIRE(std::fabs(got.eyes[1].fov_left_right_up_down[1]-0.8f)<0.00001f);
    frame.sequence=2;frame.eyes[0].position_m[1]=0.5f;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==2 && std::fabs(got.eyes[0].position_m[1]-0.5f)<0.00001f);
    REQUIRE(host.Invalidate());
    REQUIRE(!emulator.Read(got)); // transient tracking loss clears stale pose
    frame.sequence=3;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==3); // tracking recovery
    // Full original HMD pose pairs may arrive close to tracking invalidation.
    // A release/acquire seqlock must never publish a half-old eye pair.
    for(std::uint64_t id=4u;id<24u;++id){
        Frame first=frame,second=frame;
        first.sequence=second.sequence=id;
        first.eyes[0].position_m[0]=-0.03f;
        first.eyes[1].position_m[0]=0.03f;
        second.eyes[0].position_m[0]=-0.07f;
        second.eyes[1].position_m[0]=0.07f;
        std::atomic<unsigned> ready{0u};
        std::atomic<bool> go{false};
        bool wrote_first=false,wrote_second=false;
        std::thread a([&]{
            ready.fetch_add(1u);
            while(!go.load())std::this_thread::yield();
            wrote_first=host.Publish(first);
        });
        std::thread b([&]{
            ready.fetch_add(1u);
            while(!go.load())std::this_thread::yield();
            wrote_second=host.Publish(second);
        });
        while(ready.load()!=2u)std::this_thread::yield();
        go.store(true);
        a.join();b.join();
        REQUIRE(wrote_first!=wrote_second);
        REQUIRE(emulator.Read(got));
        REQUIRE(got.sequence==id);
        const float x=wrote_first?0.03f:0.07f;
        REQUIRE(std::fabs(got.eyes[0].position_m[0]+x)<0.00001f);
        REQUIRE(std::fabs(got.eyes[1].position_m[0]-x)<0.00001f);
    }
    REQUIRE(host.Invalidate());
    REQUIRE(!emulator.Read(got));
    frame.sequence=24u;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got) && got.sequence==24u);
    Frame tracked=frame;
    tracked.sequence=25u;
    tracked.reserved=7u;
    tracked.predicted_display_time=9000000;
    tracked.view_flags=kFullSixDofTracked;
    REQUIRE(host.Publish(tracked));
    REQUIRE(emulator.ReadTracked(got));
    REQUIRE(got.sequence==25u && got.reserved==7u && got.published_qpc!=0u);
    tracked.sequence=26u;tracked.reserved=0u;
    REQUIRE(host.Publish(tracked));
    REQUIRE(!emulator.ReadTracked(got));
    tracked.sequence=27u;tracked.reserved=7u;
    tracked.predicted_display_time=0;
    REQUIRE(host.Publish(tracked));
    REQUIRE(!emulator.ReadTracked(got));
    tracked.sequence=28u;tracked.predicted_display_time=9000001;
    tracked.view_flags=kFullSixDofTracked&~kPositionTracked;
    REQUIRE(host.Publish(tracked));
    REQUIRE(!emulator.ReadTracked(got));
    tracked.sequence=29u;tracked.view_flags=kFullSixDofTracked;
    REQUIRE(host.Publish(tracked));
    REQUIRE(emulator.ReadTracked(got));
    REQUIRE(got.sequence==29u && got.predicted_display_time==9000001);
    emulator.Close();host.Close();
    REQUIRE(!emulator.Read(got));
    return 0;
}
