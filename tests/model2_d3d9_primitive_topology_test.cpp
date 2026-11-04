// Source topology fidelity: no original emulator, ROM, HMD or FFB needed.
#include "../src/probe/d3d9_primitive_topology.hpp"
#include <cstdint>
#include <cstdio>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"original topology line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    CHECK(PrimitiveSourceElementCount(D3DPT_TRIANGLELIST,2)==6u);
    CHECK(PrimitiveSourceElementCount(D3DPT_TRIANGLESTRIP,2)==4u);
    CHECK(PrimitiveSourceElementCount(D3DPT_TRIANGLEFAN,3)==5u);
    CHECK(PrimitiveSourceElementCount(D3DPT_LINELIST,2)==0u);
    CHECK(PrimitiveSourceElementCount(D3DPT_TRIANGLESTRIP,0)==0u);
    CHECK(PrimitiveSourceElementCount(D3DPT_TRIANGLELIST,683)==0u);
    std::vector<std::uint32_t> expanded;
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLELIST,2,
        {20u,21u,22u,25u,26u,27u},expanded));
    CHECK(expanded==std::vector<std::uint32_t>(
        {20,21,22,25,26,27}));
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLESTRIP,3,
        {0u,1u,2u,3u,4u},expanded));
    CHECK(expanded==std::vector<std::uint32_t>({
        0,1,2,2,1,3,2,3,4})); // winding parity is preserved
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLEFAN,3,
        {7u,8u,9u,10u,11u},expanded));
    CHECK(expanded==std::vector<std::uint32_t>({
        7,8,9,7,9,10,7,10,11}));
    const auto previous=expanded;
    CHECK(!ExpandOriginalTriangles(D3DPT_TRIANGLEFAN,3,
        {7u,8u,9u},expanded));
    CHECK(expanded==previous);
    CHECK(!ExpandOriginalTriangles(D3DPT_POINTLIST,1,
        {7u},expanded));
    CHECK(expanded==previous);
    CHECK(!ExpandOriginalTriangles(D3DPT_TRIANGLESTRIP,683,
        std::vector<std::uint32_t>(685u,1u),expanded));
    CHECK(expanded==previous);
    return 0;
}
