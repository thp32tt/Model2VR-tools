#pragma once
// Pure, bounded D3D9 triangle-list index-window validation. The game owns
// the buffers; this code only validates a previously copied index slice.
#include <d3d9.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace m2vr::drawprobe {
inline constexpr std::uint32_t kMaxVBBridgeVertices=2048u;
inline constexpr std::uint32_t kMaxVBBridgeTriangles=682u;

struct VBVertexWindow {
    std::uint32_t first=0, count=0;
    std::vector<std::uint32_t> indices; // relative to first
};

inline bool RemapIndexedTriangleList(
    const std::uint8_t* bytes, std::size_t available,
    D3DFORMAT format, std::int32_t base_vertex,
    std::uint32_t min_index, std::uint32_t num_vertices,
    std::uint32_t triangle_count, VBVertexWindow& out) {
    if(!bytes || (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32) ||
       !triangle_count || triangle_count>kMaxVBBridgeTriangles ||
       !num_vertices || num_vertices>65536u ||
       std::uint64_t(min_index)+num_vertices>0x100000000ULL)
        return false;
    const std::size_t width=format==D3DFMT_INDEX16?2u:4u;
    const std::size_t elements=std::size_t(triangle_count)*3u;
    if(elements*width>available)return false;
    std::vector<std::uint32_t> absolute;
    absolute.reserve(elements);
    std::uint32_t low=UINT32_MAX,high=0;
    const auto upper=std::uint64_t(min_index)+num_vertices;
    for(std::size_t i=0;i<elements;++i){
        std::uint32_t index=0;
        if(width==2u){
            std::uint16_t x=0;
            std::memcpy(&x,bytes+i*width,width);
            index=x;
        }else{
            std::memcpy(&index,bytes+i*width,width);
        }
        if(index<min_index || std::uint64_t(index)>=upper)return false;
        // D3D9 DrawIndexedPrimitive addresses vertex at base + index.
        const std::int64_t vertex=std::int64_t(base_vertex)+index;
        if(vertex<0 || vertex>UINT32_MAX)return false;
        const auto actual=static_cast<std::uint32_t>(vertex);
        low=std::min(low,actual);
        high=std::max(high,actual);
        absolute.push_back(actual);
    }
    const auto span=std::uint64_t(high)-low+1u;
    if(span>kMaxVBBridgeVertices)return false;
    VBVertexWindow result{};
    result.first=low;
    result.count=static_cast<std::uint32_t>(span);
    result.indices.reserve(elements);
    for(const auto v:absolute)result.indices.push_back(v-low);
    out=std::move(result);
    return true;
}

inline bool RemapSequentialTriangleList(
    std::uint32_t start_vertex,std::uint32_t triangle_count,
    VBVertexWindow& out) {
    if(!triangle_count || triangle_count>kMaxVBBridgeTriangles)return false;
    const std::uint32_t count=triangle_count*3u;
    if(std::uint64_t(start_vertex)+count>0x100000000ULL ||
       count>kMaxVBBridgeVertices)return false;
    VBVertexWindow result{};
    result.first=start_vertex;
    result.count=count;
    result.indices.reserve(count);
    for(std::uint32_t i=0;i<count;++i)result.indices.push_back(i);
    out=std::move(result);
    return true;
}
} // namespace m2vr::drawprobe
