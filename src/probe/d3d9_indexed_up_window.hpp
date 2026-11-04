#pragma once
// Original D3D9 DrawIndexedPrimitiveUP: pVertexStreamZeroData begins at
// MinVertexIndex, not vertex zero. This alone never proves game-world pass.
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
namespace m2vr::drawprobe {
inline constexpr std::uint32_t kMaxIndexedUPVertices=2048u;
inline constexpr std::uint32_t kMaxIndexedUPTriangles=682u;
inline bool IndexedUPRelativeIndex(std::uint32_t original,
    std::uint32_t min_vertex,std::uint32_t vertex_count,
    std::uint32_t& relative) noexcept {
    if(!vertex_count || vertex_count>kMaxIndexedUPVertices ||
       std::uint64_t(min_vertex)+vertex_count>0x100000000ULL ||
       original<min_vertex ||
       std::uint64_t(original)>=std::uint64_t(min_vertex)+vertex_count)
        return false;
    relative=original-min_vertex;
    return true;
}
inline bool RemapIndexedUPTriangleList(
    const std::uint8_t* data,std::size_t available,D3DFORMAT format,
    std::uint32_t min_vertex,std::uint32_t vertex_count,
    std::uint32_t triangles,std::vector<std::uint32_t>& output) {
    if(!data || !triangles || triangles>kMaxIndexedUPTriangles ||
       vertex_count>kMaxIndexedUPVertices ||
       (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32))
        return false;
    const std::size_t width=format==D3DFMT_INDEX16?2u:4u;
    const std::size_t count=std::size_t(triangles)*3u;
    if(count*width>available)return false;
    std::vector<std::uint32_t> next;
    next.reserve(count);
    for(std::size_t i=0;i<count;++i){
        std::uint32_t absolute=0u,relative=0u;
        if(width==2u){
            std::uint16_t short_index=0u;
            std::memcpy(&short_index,data+i*width,width);
            absolute=short_index;
        }else std::memcpy(&absolute,data+i*width,width);
        if(!IndexedUPRelativeIndex(absolute,min_vertex,vertex_count,relative))
            return false;
        next.push_back(relative);
    }
    output=std::move(next);
    return true;
}
} // namespace m2vr::drawprobe
