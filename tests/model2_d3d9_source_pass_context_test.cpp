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
    auto bits=EncodeOriginalDrawSourceContext(c);
    CHECK((bits&kSourceTargetMask)==2u);
    CHECK((bits&kSourceViewportKnown)!=0u);
    CHECK((bits&kSourceViewportFull)==0u);
    CHECK((bits&kSourceDepthKnown)!=0u);
    CHECK((bits&kSourceDepthEnabled)!=0u);
    CHECK((bits&kSourceDepthWrite)!=0u);
    CHECK((bits&kSourceScissorEnabled)!=0u);
    c.target=OriginalTargetClass::MainBackbuffer;
    bits=EncodeOriginalDrawSourceContext(c);
    CHECK((bits&kSourceTargetMask)==1u);
    CHECK((bits&kSourceViewportFull)!=0u);
    c.viewport_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceViewportFull)==0u);
    c.viewport_known=true;c.depth_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceDepthEnabled)==0u);
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceDepthWrite)==0u);
    c.scissor_known=false;
    CHECK((EncodeOriginalDrawSourceContext(c)&kSourceScissorEnabled)==0u);
    return 0;
}
