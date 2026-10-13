#include "../src/common/stereo_pose_ipc.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
    REQUIRE(got.eyes[0].position_m[0]<0 && got.eyes[1].position_m[0]>0);
    REQUIRE(std::fabs(got.eyes[1].fov_left_right_up_down[1]-0.8f)<0.00001f);
    frame.sequence=2;frame.eyes[0].position_m[1]=0.5f;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==2 && std::fabs(got.eyes[0].position_m[1]-0.5f)<0.00001f);
    emulator.Close();host.Close();
    REQUIRE(!emulator.Read(got));
    return 0;
}
