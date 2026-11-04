#include "../src/probe/d3d9_source_pass_context.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"original source-pass context line %d\n",__LINE__);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    OriginalDrawSourceContext c{};
    CHECK(EncodeOriginalDrawSourceContext(c)==0u);
    c.target=OriginalTargetClass::Auxiliary;
    c.viewport_known=true;c.viewport_full=true;
    c.depth_known=true;c.depth_enabled=true;c.depth_write=true;
    c.scissor_known=true;c.scissor_enabled=true;
    c.target_desc_known=true;c.source_width=256u;c.source_height=256u;
    c.depth_surface_known=true;c.depth_surface_compatible=true;
    auto bits=EncodeOriginalDrawSourceContext(c);
    CHECK((bits&kSourceTargetMask)==2u);
    CHECK((bits&kSourceViewportKnown)!=0u);
    CHECK((bits&kSourceViewportFull)==0u);
    CHECK((bits&kSourceDepthKnown)!=0u);
    CHECK((bits&kSourceDepthEnabled)!=0u);
    CHECK((bits&kSourceDepthWrite)!=0u);
    CHECK((bits&kSourceScissorEnabled)!=0u);
    CHECK((bits&kSourceDepthSurfaceKnown)!=0u);
    CHECK((bits&kSourceDepthSurfaceCompatible)!=0u);
    CHECK((bits&kSourceScissorRectKnown)==0u);
    CHECK((bits&kSourceScissorFull)==0u);
    c.scissor_rect_known=true;c.scissor_full=true;
    bits=EncodeOriginalDrawSourceContext(c);
    CHECK((bits&kSourceScissorRectKnown)!=0u);
    CHECK((bits&kSourceScissorFull)==0u); // auxiliary cannot be full main
    c.target=OriginalTargetClass::MainBackbuffer;
    bits=EncodeOriginalDrawSourceContext(c);
    CHECK((bits&kSourceTargetMask)==1u);
    CHECK((bits&kSourceViewportFull)!=0u);
    CHECK((bits&kSourceScissorFull)!=0u);
    c.scissor_full=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceScissorFull)==0u);
    c.scissor_full=true;
    c.scissor_rect_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceScissorFull)==0u);
    c.scissor_rect_known=true;
    c.target_desc_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceViewportFull)==0u);
    c.target_desc_known=true;
    c.source_width=0u;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceViewportFull)==0u);
    c.source_width=256u;c.source_height=0u;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceViewportFull)==0u);
    c.source_height=256u;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceViewportFull)!=0u);
    c.viewport_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceViewportFull)==0u);
    c.depth_surface_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceDepthSurfaceCompatible)==0u);
    c.depth_surface_known=true;c.depth_surface_compatible=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceDepthSurfaceCompatible)==0u);
    c.viewport_known=true;c.depth_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceDepthEnabled)==0u);
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceDepthWrite)==0u);
    c.scissor_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceScissorEnabled)==0u);
    return 0;
}
