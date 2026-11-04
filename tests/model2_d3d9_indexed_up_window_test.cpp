#include "../src/probe/d3d9_indexed_up_window.hpp"
#include "../src/probe/d3d9_vb_index_window.hpp"
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
    CHECK(!IndexedUPRelativeIndex(0u,0u,2049u,relative));
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
    return 0;
}
