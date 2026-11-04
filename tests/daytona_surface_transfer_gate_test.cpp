#include "../src/probe/daytona_surface_transfer_gate.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    using namespace m2vr::daytona;
    RECT full{0,0,640,480},partial{0,0,320,240},wrong{-1,0,640,480};
    REQUIRE(FullSurfaceRect(nullptr,640,480));
    REQUIRE(FullSurfaceRect(&full,640,480));
    REQUIRE(!FullSurfaceRect(&partial,640,480));
    REQUIRE(!FullSurfaceRect(&wrong,640,480));
    REQUIRE(!FullSurfaceRect(nullptr,0,480));
    REQUIRE(CanMirrorSurfaceTransfer(true,true,true,false));
    REQUIRE(CanMirrorSurfaceTransfer(true,true,false,true));
    REQUIRE(!CanMirrorSurfaceTransfer(true,true,false,false));
    REQUIRE(!CanMirrorSurfaceTransfer(false,true,true,true));
    REQUIRE(!CanMirrorSurfaceTransfer(true,false,true,true));
    return 0;
}
