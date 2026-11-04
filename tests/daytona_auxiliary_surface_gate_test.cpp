#include "../src/probe/daytona_auxiliary_surface_gate.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"aux chain FAIL %d\n",__LINE__);std::exit(1);}}while(0)
int main(){
    using namespace m2vr::daytona;
    D3DSURFACE_DESC main{},offscreen{},depth{};
    main.Pool=D3DPOOL_DEFAULT;main.Usage=D3DUSAGE_RENDERTARGET;
    main.Width=800;main.Height=600;main.Format=D3DFMT_A8R8G8B8;
    main.MultiSampleType=D3DMULTISAMPLE_NONE;main.MultiSampleQuality=0;
    offscreen=main;
    depth.Pool=D3DPOOL_DEFAULT;depth.Usage=D3DUSAGE_DEPTHSTENCIL;
    depth.Width=800;depth.Height=600;depth.Format=D3DFMT_D24S8;
    depth.MultiSampleType=D3DMULTISAMPLE_NONE;depth.MultiSampleQuality=0;
    CHECK(SourceCompatibleWithMain(offscreen,depth,main));
    auto smaller=offscreen;smaller.Width=400;
    CHECK(!SourceCompatibleWithMain(smaller,depth,main));
    smaller=offscreen;smaller.Usage=0;
    CHECK(!SourceCompatibleWithMain(smaller,depth,main));
    smaller=offscreen;smaller.Format=D3DFMT_X8R8G8B8;
    CHECK(!SourceCompatibleWithMain(smaller,depth,main));
    smaller=offscreen;smaller.MultiSampleType=D3DMULTISAMPLE_4_SAMPLES;
    CHECK(!SourceCompatibleWithMain(smaller,depth,main));
    auto mismatch=depth;mismatch.MultiSampleQuality=1;
    CHECK(!SourceCompatibleWithMain(offscreen,mismatch,main));
    mismatch=depth;mismatch.Usage=0;
    CHECK(!SourceCompatibleWithMain(offscreen,mismatch,main));
    // A game frame may begin with a full-size auxiliary 3D render target.
    CHECK(BootstrapTargetCompatible(offscreen,depth,main));
    CHECK(BootstrapTargetCompatible(main,depth,main));
    auto small_aux=offscreen;small_aux.Width=320;
    CHECK(!BootstrapTargetCompatible(small_aux,depth,main));
    auto other_depth=depth;other_depth.Format=D3DFMT_D16;
    CHECK(SameEyeDepthLayout(depth,depth));
    CHECK(!SameEyeDepthLayout(depth,other_depth));
    other_depth=depth;other_depth.MultiSampleQuality=1;
    CHECK(!SameEyeDepthLayout(depth,other_depth));
    D3DRECT full{0,0,800,600},partial{0,0,300,600};
    const D3DVIEWPORT9 view{0,0,800,600,0.f,1.f};
    CHECK(FullOriginalViewport(view,800,600));
    auto subview=view;subview.Width=400;
    CHECK(!FullOriginalViewport(subview,800,600));
    subview=view;subview.Y=50;
    CHECK(!FullOriginalViewport(subview,800,600));
    CHECK(FullInitialColorClear(0,nullptr,D3DCLEAR_TARGET,800,600));
    CHECK(FullInitialColorClear(1,&full,D3DCLEAR_TARGET,800,600));
    CHECK(!FullInitialColorClear(1,&partial,D3DCLEAR_TARGET,800,600));
    CHECK(!FullInitialColorClear(0,nullptr,D3DCLEAR_ZBUFFER,800,600));
    CHECK(!FullInitialColorClear(2,&full,D3DCLEAR_TARGET,800,600));
    CHECK(!FullInitialColorClear(0,nullptr,D3DCLEAR_TARGET,0,600));
    CHECK(RouteOriginalToMain(true,false,false,false,true,false)==
          MainBlitSource::FlatSource);
    CHECK(RouteOriginalToMain(true,false,true,true,true,false)==
          MainBlitSource::StereoAuxiliary);
    CHECK(RouteOriginalToMain(true,false,true,false,true,false)==
          MainBlitSource::Skip);
    CHECK(RouteOriginalToMain(true,true,false,true,true,true)==
          MainBlitSource::Skip);
    CHECK(RouteOriginalToMain(false,false,true,true,true,true)==
          MainBlitSource::Skip);
    CHECK(RouteOriginalToMain(true,false,true,true,false,false)==
          MainBlitSource::Skip);
    CHECK(RouteOriginalToMain(true,false,true,true,false,true)==
          MainBlitSource::StereoAuxiliary);
}
