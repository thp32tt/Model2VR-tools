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
inline constexpr std::uint32_t kMaxIndexedUPDeclaredVertices=65536u;
inline constexpr std::uint32_t kMaxIndexedUPCopiedVertices=2048u;
inline constexpr std::uint32_t kMaxIndexedUPTriangles=682u;
inline bool IndexedUPRelativeIndex(std::uint32_t original,
    std::uint32_t min_vertex,std::uint32_t vertex_count,
    std::uint32_t& relative) noexcept {
    if(!vertex_count || vertex_count>kMaxIndexedUPDeclaredVertices ||
       std::uint64_t(min_vertex)+vertex_count>0x100000000ULL ||
       original<min_vertex ||
       std::uint64_t(original)>=std::uint64_t(min_vertex)+vertex_count)
        return false;
    relative=original-min_vertex;
    return true;
}
// Copy only the original index sequence for list/strip/fan, never infer
// source-world proof. The D3D9 UP pointer addresses MinVertexIndex.
inline bool RemapIndexedUPSourceElements(
    const std::uint8_t* data,std::size_t available,D3DFORMAT format,
    std::uint32_t min_vertex,std::uint32_t vertex_count,
    std::uint32_t source_count,std::vector<std::uint32_t>& output) {
    if(!data || source_count<3u ||
       source_count>kMaxIndexedUPTriangles*3u ||
       !vertex_count || vertex_count>kMaxIndexedUPDeclaredVertices ||
       (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32))
        return false;
    const std::size_t width=format==D3DFMT_INDEX16?2u:4u;
    if(std::size_t(source_count)*width>available)return false;
    std::vector<std::uint32_t> next;
    next.reserve(source_count);
    for(std::uint32_t i=0u;i<source_count;++i){
        std::uint32_t absolute=0u,relative=0u;
        if(width==2u){
            std::uint16_t idx=0u;
            std::memcpy(&idx,data+std::size_t(i)*width,width);
            absolute=idx;
        }else std::memcpy(&absolute,data+std::size_t(i)*width,width);
        if(!IndexedUPRelativeIndex(absolute,min_vertex,vertex_count,relative))
            return false;
        next.push_back(relative);
    }
    output=std::move(next);
    return true;
}
inline bool RemapIndexedUPTriangleList(
    const std::uint8_t* data,std::size_t available,D3DFORMAT format,
    std::uint32_t min_vertex,std::uint32_t vertex_count,
    std::uint32_t triangles,std::vector<std::uint32_t>& output) {
    return triangles && triangles<=kMaxIndexedUPTriangles &&
        RemapIndexedUPSourceElements(data,available,format,
            min_vertex,vertex_count,triangles*3u,output);
}
// Extract the smallest contiguous source span actually referenced by this
// original Draw. The user's UP memory still starts at MinVertexIndex.
// Reject huge sparse spans; never read unrelated vertices or past-end data.
struct IndexedUPCopiedWindow {
    std::uint32_t source_offset=0;
    std::uint32_t count=0;
    std::vector<std::uint32_t> indices;
};
inline bool CompactIndexedUPIndexWindow(
    const std::vector<std::uint32_t>& relative,
    std::uint32_t declared_vertices,IndexedUPCopiedWindow& output) {
    if(relative.empty() || relative.size()%3u ||
       relative.size()>std::size_t(kMaxIndexedUPTriangles)*3u ||
       !declared_vertices || declared_vertices>kMaxIndexedUPDeclaredVertices)
        return false;
    std::uint32_t low=UINT32_MAX,high=0u;
    for(const auto index:relative) {
        if(index>=declared_vertices)return false;
        if(index<low)low=index;
        if(index>high)high=index;
    }
    const auto span=std::uint64_t(high)-low+1u;
    if(!span || span>kMaxIndexedUPCopiedVertices)return false;
    IndexedUPCopiedWindow next{};
    next.source_offset=low;
    next.count=static_cast<std::uint32_t>(span);
    next.indices.reserve(relative.size());
    for(const auto index:relative)next.indices.push_back(index-low);
    output=std::move(next);
    return true;
}
} // namespace m2vr::drawprobe
