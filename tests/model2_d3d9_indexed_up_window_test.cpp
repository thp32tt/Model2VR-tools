#include "../src/probe/d3d9_indexed_up_window.hpp"
#include "../src/probe/d3d9_vb_index_window.hpp"
#include "../src/probe/d3d9_primitive_topology.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"indexed-UP window line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    std::uint32_t relative=99u;
    CHECK(IndexedUPRelativeIndex(12u,10u,4u,relative)&&relative==2u);
    CHECK(!IndexedUPRelativeIndex(9u,10u,4u,relative)&&relative==2u);
    CHECK(!IndexedUPRelativeIndex(14u,10u,4u,relative));
    CHECK(!IndexedUPRelativeIndex(0u,0u,0u,relative));
    CHECK(!IndexedUPRelativeIndex(0u,0u,65537u,relative));
    CHECK(IndexedUPRelativeIndex(UINT32_MAX,UINT32_MAX-2u,3u,relative));
    CHECK(relative==2u);
    CHECK(!IndexedUPRelativeIndex(0u,UINT32_MAX,3u,relative));
    const std::array<std::uint16_t,6> source16{10,11,12,12,11,13};
    std::vector<std::uint32_t> order;
    CHECK(RemapIndexedUPTriangleList(
        reinterpret_cast<const std::uint8_t*>(source16.data()),
        sizeof(source16),D3DFMT_INDEX16,10u,4u,2u,order));
    const std::vector<std::uint32_t> expected{0,1,2,2,1,3};
    CHECK(order==expected);
    CHECK(!RemapIndexedUPTriangleList(
        reinterpret_cast<const std::uint8_t*>(source16.data()),
        sizeof(source16)-1u,D3DFMT_INDEX16,10u,4u,2u,order));
    CHECK(order==expected); // no partial output on failed remap
    CHECK(!RemapIndexedUPTriangleList(
        reinterpret_cast<const std::uint8_t*>(source16.data()),
        sizeof(source16),D3DFMT_UNKNOWN,10u,4u,2u,order));
    CHECK(!RemapIndexedUPTriangleList(
        reinterpret_cast<const std::uint8_t*>(source16.data()),
        sizeof(source16),D3DFMT_INDEX16,11u,3u,2u,order));
    const std::array<std::uint32_t,6> source32{65536,65537,65538,65538,65537,65539};
    CHECK(RemapIndexedUPTriangleList(
        reinterpret_cast<const std::uint8_t*>(source32.data()),
        sizeof(source32),D3DFMT_INDEX32,65536u,4u,2u,order));
    CHECK(order==expected);
    CHECK(!RemapIndexedUPTriangleList(
        reinterpret_cast<const std::uint8_t*>(source32.data()),
        sizeof(source32),D3DFMT_INDEX32,UINT32_MAX-1u,4u,2u,order));
    const std::array<std::array<float,6>,4> projected{{
        {{100.f,120.f,0.5f,0.25f,0.f,0.f}},
        {{200.f,120.f,0.5f,0.25f,1.f,0.f}},
        {{100.f,220.f,0.5f,0.25f,0.f,1.f}},
        {{200.f,220.f,0.5f,0.25f,1.f,1.f}}
    }};
    VBVertexWindow window{};
    window.first=0u;window.count=4u;window.indices=expected;
    std::vector<std::uint8_t> expanded;
    constexpr auto fvf=D3DFVF_XYZRHW|D3DFVF_TEX1;
    CHECK(ExpandProjectedVBWindow(
        reinterpret_cast<const std::uint8_t*>(projected.data()),
        sizeof(projected),fvf,24u,window,expanded));
    CHECK(expanded.size()==144u);
    std::array<float,6> corner{};
    std::memcpy(corner.data(),expanded.data()+3u*24u,24u);
    CHECK(corner[0]==100.f && corner[1]==220.f);
    const auto previous=expanded;
    window.indices[1]=4u;
    CHECK(!ExpandProjectedVBWindow(
        reinterpret_cast<const std::uint8_t*>(projected.data()),
        sizeof(projected),fvf,24u,window,expanded));
    CHECK(expanded==previous);
    // Legal large declared source windows can hold small, sparse
    // original triangle ranges. Only referenced original bytes are read.
    const std::vector<std::uint32_t> sparse{200u,201u,202u,202u,201u,203u};
    IndexedUPCopiedWindow compact{};
    CHECK(CompactIndexedUPIndexWindow(sparse,50000u,compact));
    CHECK(compact.source_offset==200u && compact.count==4u);
    CHECK(compact.indices==expected);
    const auto old=compact.indices;
    CHECK(!CompactIndexedUPIndexWindow(
        std::vector<std::uint32_t>{0u,3000u,1u},50000u,compact));
    CHECK(compact.indices==old);
    CHECK(!CompactIndexedUPIndexWindow(sparse,203u,compact));
    CHECK(!CompactIndexedUPIndexWindow(sparse,0u,compact));
    CHECK(!CompactIndexedUPIndexWindow(
        std::vector<std::uint32_t>{1u,2u},50000u,compact));
    // Indexed-UP strip/fan source indices are absolute while the copied
    // vertex memory starts at MinVertexIndex; triangulate before compaction.
    const std::array<std::uint16_t,4> strip{501u,502u,503u,504u};
    std::vector<std::uint32_t> ordered_source,topology;
    CHECK(RemapIndexedUPSourceElements(
        reinterpret_cast<const std::uint8_t*>(strip.data()),
        sizeof(strip),D3DFMT_INDEX16,500u,10000u,4u,ordered_source));
    CHECK(ordered_source==std::vector<std::uint32_t>({1,2,3,4}));
    CHECK(ExpandOriginalTriangles(
        D3DPT_TRIANGLESTRIP,2u,ordered_source,topology));
    CHECK(topology==std::vector<std::uint32_t>({1,2,3,3,2,4}));
    CHECK(CompactIndexedUPIndexWindow(topology,10000u,compact));
    CHECK(compact.source_offset==1u && compact.count==4u);
    CHECK(compact.indices==std::vector<std::uint32_t>({0,1,2,2,1,3}));
    CHECK(ExpandOriginalTriangles(
        D3DPT_TRIANGLEFAN,2u,ordered_source,topology));
    CHECK(topology==std::vector<std::uint32_t>({1,2,3,1,3,4}));
    CHECK(!RemapIndexedUPSourceElements(
        reinterpret_cast<const std::uint8_t*>(strip.data()),
        sizeof(strip)-1u,D3DFMT_INDEX16,500u,10000u,4u,ordered_source));
    CHECK(ordered_source==std::vector<std::uint32_t>({1,2,3,4}));
    // Real projected indexed-UP DIFFUSE and optional SPECULAR source:
    // 32-bit packed colors are not position floats; reordered triangle
    // vertices must keep true ARGB, RHW, and float2 UV for both XR eyes.
    const auto argb_fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|
                        D3DFVF_SPECULAR|D3DFVF_TEX1;
    std::array<std::array<std::uint8_t,32>,4> color_vertices{};
    for(std::size_t i=0;i<color_vertices.size();++i){
        const float pos[4]={100.f+float(i)*50.f,140.f,0.5f,0.25f};
        const std::uint32_t diffuse_color=0x11000000u |
                                           (std::uint32_t(i)<<16u);
        const std::uint32_t specular_color=0xffffffffu;
        const float texture_uv[2]={float(i)*0.25f,0.75f};
        std::memcpy(color_vertices[i].data(),pos,sizeof(pos));
        std::memcpy(color_vertices[i].data()+16u,&diffuse_color,4u);
        std::memcpy(color_vertices[i].data()+20u,&specular_color,4u);
        std::memcpy(color_vertices[i].data()+24u,texture_uv,sizeof(texture_uv));
    }
    window.first=0u;window.count=4u;window.indices=expected;
    CHECK(ExpandProjectedVBWindow(
        reinterpret_cast<const std::uint8_t*>(color_vertices.data()),
        sizeof(color_vertices),argb_fvf,32u,window,expanded));
    CHECK(expanded.size()==192u);
    for(std::size_t i=0;i<6u;++i){
        ProjectedUPWorldVertex projected_corner{};
        CHECK(DecodeProjectedUPWorldVertex(
            expanded.data()+i*32u,argb_fvf,32u,projected_corner));
        const std::size_t source_index=expected[i];
        CHECK(projected_corner.has_diffuse &&
              projected_corner.has_specular &&
              projected_corner.diffuse_argb==
                  (0x11000000u|(std::uint32_t(source_index)<<16u)) &&
              projected_corner.uv[0]==float(source_index)*0.25f &&
              projected_corner.uv[1]==0.75f);
    }
    const auto last_valid=expanded;
    CHECK(!ExpandProjectedVBWindow(
        reinterpret_cast<const std::uint8_t*>(color_vertices.data()),
        sizeof(color_vertices)-1u,argb_fvf,32u,window,expanded));
    CHECK(expanded==last_valid);
    return 0;
}
