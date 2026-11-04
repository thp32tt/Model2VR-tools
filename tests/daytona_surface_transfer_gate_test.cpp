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
    bool full_update=false;
    REQUIRE(UpdateSurfaceBounds(nullptr,640,480,nullptr,640,480,full_update));
    REQUIRE(full_update);
    POINT offset{10,10};
    REQUIRE(UpdateSurfaceBounds(nullptr,320,200,&offset,640,480,full_update));
    REQUIRE(!full_update);
    REQUIRE(!UpdateSurfaceBounds(nullptr,640,480,&offset,640,480,full_update));
    POINT negative{-1,0};
    REQUIRE(!UpdateSurfaceBounds(nullptr,320,200,&negative,640,480,full_update));
    RECT crop{0,0,100,100};
    REQUIRE(UpdateSurfaceBounds(&crop,640,480,nullptr,640,480,full_update));
    REQUIRE(!full_update);
    RECT overflow{0,0,641,480};
    REQUIRE(!UpdateSurfaceBounds(&overflow,640,480,nullptr,640,480,full_update));
    RECT bad{-1,0,100,100};
    REQUIRE(!UpdateSurfaceBounds(&bad,640,480,nullptr,640,480,full_update));
    return 0;
}
