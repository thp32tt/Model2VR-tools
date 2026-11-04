// No emulator, ROM, headset or FFB required: verify exact D3D9 VB/IB
// index remapping before enabling the external verified-world adapter.
#include "../src/probe/d3d9_vb_index_window.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr, "VB window failed at line %d: %s\n",__LINE__,#x);return 1;}}while(0)

using m2vr::drawprobe::VBVertexWindow;
using m2vr::drawprobe::RemapIndexedTriangleList;
using m2vr::drawprobe::RemapSequentialTriangleList;
int main(){
    const std::array<std::uint16_t,6> a{5,6,7,7,8,5};
    VBVertexWindow w{};
    CHECK(RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(a.data()),sizeof(a),
        D3DFMT_INDEX16,-2,5,4,2,w));
    CHECK(w.first==3 && w.count==4 &&
          w.indices==std::vector<std::uint32_t>({0,1,2,2,3,0}));
    // Failures must not publish a partially remapped source window.
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(a.data()),sizeof(a)-1,
        D3DFMT_INDEX16,-2,5,4,2,w));
    CHECK(w.first==3 && w.count==4 && w.indices.size()==6u);
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(a.data()),sizeof(a),
        D3DFMT_INDEX16,-2,6,3,2,w)); // a real index below MinVertexIndex
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(a.data()),sizeof(a),
        D3DFMT_INDEX16,-7,5,4,2,w)); // effective vertex negative
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(a.data()),sizeof(a),
        D3DFMT_INDEX16,0,5,4,0,w));
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(a.data()),sizeof(a),
        D3DFMT_UNKNOWN,0,5,4,2,w));
    const std::array<std::uint32_t,3> wide{65536,65537,65538};
    CHECK(RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(wide.data()),sizeof(wide),
        D3DFMT_INDEX32,-65500,65536,3,1,w));
    CHECK(w.first==36 && w.count==3 &&
          w.indices==std::vector<std::uint32_t>({0,1,2}));
    const std::array<std::uint32_t,3> spread{1,3000,2};
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(spread.data()),
        sizeof(spread),D3DFMT_INDEX32,0,0,3001,1,w)); // >2048 span
    const std::array<std::uint32_t,3> overflow{
        UINT32_MAX-2,UINT32_MAX-1,UINT32_MAX};
    // The last legal UINT32 vertex is valid. Adding a signed base
    // must still reject overflow rather than silently wrapping to zero.
    CHECK(RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(overflow.data()),
        sizeof(overflow),D3DFMT_INDEX32,0,UINT32_MAX-2,3,1,w));
    CHECK(w.first==UINT32_MAX-2 && w.count==3 &&
          w.indices==std::vector<std::uint32_t>({0,1,2}));
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(overflow.data()),
        sizeof(overflow),D3DFMT_INDEX32,2,UINT32_MAX-2,3,1,w));
    CHECK(!RemapIndexedTriangleList(
        reinterpret_cast<const std::uint8_t*>(overflow.data()),
        sizeof(overflow),D3DFMT_INDEX32,0,UINT32_MAX-1,3,1,w));
    CHECK(RemapSequentialTriangleList(100,2,w));
    CHECK(w.first==100 && w.count==6 && w.indices[0]==0 &&
          w.indices[5]==5);
    CHECK(RemapSequentialTriangleList(0,682,w));
    CHECK(w.count==2046 && w.indices.size()==2046u);
    CHECK(!RemapSequentialTriangleList(0,683,w));
    CHECK(!RemapSequentialTriangleList(UINT32_MAX-1,1,w));
    CHECK(!RemapSequentialTriangleList(0,0,w));

    // Original indexed VB order (including repeated corners) is preserved
    // before the authenticated CPU projector's exact screen-XY join.
    constexpr auto projected_fvf=D3DFVF_XYZRHW|D3DFVF_TEX1;
    const std::array<std::array<float,6>,4> projected_vertices{{
        {{120.f,140.f,0.4f,0.5f,0.f,0.f}},
        {{250.f,140.f,0.4f,0.5f,1.f,0.f}},
        {{120.f,260.f,0.5f,0.4f,0.f,1.f}},
        {{250.f,260.f,0.5f,0.4f,1.f,1.f}}
    }};
    VBVertexWindow projected{};
    projected.first=7u;
    projected.count=4u;
    projected.indices={0u,1u,2u,2u,1u,3u};
    std::vector<std::uint8_t> ordered{};
    const auto* raw=reinterpret_cast<const std::uint8_t*>(
        projected_vertices.data());
    CHECK(m2vr::drawprobe::ExpandProjectedVBWindow(
        raw,sizeof(projected_vertices),projected_fvf,24u,
        projected,ordered));
    CHECK(ordered.size()==144u);
    std::array<float,6> ordered_corner{};
    std::memcpy(ordered_corner.data(),ordered.data()+3u*24u,24u);
    CHECK(ordered_corner[0]==120.f && ordered_corner[1]==260.f);
    std::memcpy(ordered_corner.data(),ordered.data()+5u*24u,24u);
    CHECK(ordered_corner[0]==250.f && ordered_corner[1]==260.f);
    const auto good=ordered;
    CHECK(!m2vr::drawprobe::ExpandProjectedVBWindow(
        raw,sizeof(projected_vertices)-1u,projected_fvf,24u,
        projected,ordered));
    CHECK(ordered==good); // no partial output on failure
    CHECK(!m2vr::drawprobe::ExpandProjectedVBWindow(
        raw,sizeof(projected_vertices),D3DFVF_XYZ|D3DFVF_TEX1,24u,
        projected,ordered));
    projected.indices[5]=4u;
    CHECK(!m2vr::drawprobe::ExpandProjectedVBWindow(
        raw,sizeof(projected_vertices),projected_fvf,24u,
        projected,ordered));
    projected.indices[5]=3u;
    auto invalid=projected_vertices;
    invalid[3][3]=0.f; // projected RHW must be finite and positive
    CHECK(!m2vr::drawprobe::ExpandProjectedVBWindow(
        reinterpret_cast<const std::uint8_t*>(invalid.data()),
        sizeof(invalid),projected_fvf,24u,projected,ordered));
    CHECK(ordered==good);
    projected.indices.resize(3u); // < 2 source triangles
    CHECK(!m2vr::drawprobe::ExpandProjectedVBWindow(
        raw,sizeof(projected_vertices),projected_fvf,24u,
        projected,ordered));
    return 0;
}
