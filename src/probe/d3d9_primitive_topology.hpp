#pragma once
// Original D3D9 triangle-list/strip/fan primitive order -> triangle list.
// Strip odd triangles flip winding; fan triangles share the first vertex.
// No classification, state mutation or GPU replay is performed here.
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
namespace m2vr::drawprobe {
inline constexpr unsigned kMaxWorldPrimitiveTriangles=682u;
inline std::uint32_t PrimitiveSourceElementCount(
    D3DPRIMITIVETYPE type,unsigned triangles) noexcept {
    if(!triangles || triangles>kMaxWorldPrimitiveTriangles)return 0u;
    if(type==D3DPT_TRIANGLELIST)return triangles*3u;
    if(type==D3DPT_TRIANGLESTRIP || type==D3DPT_TRIANGLEFAN)
        return triangles+2u;
    return 0u;
}
inline bool ExpandOriginalTriangles(
    D3DPRIMITIVETYPE type,unsigned triangles,
    const std::vector<std::uint32_t>& source_indices,
    std::vector<std::uint32_t>& triangle_indices) {
    const auto needed=PrimitiveSourceElementCount(type,triangles);
    if(!needed || source_indices.size()!=needed)return false;
    std::vector<std::uint32_t> result;
    result.reserve(std::size_t(triangles)*3u);
    for(std::uint32_t i=0u;i<triangles;++i) {
        if(type==D3DPT_TRIANGLELIST) {
            result.push_back(source_indices[std::size_t(i)*3u]);
            result.push_back(source_indices[std::size_t(i)*3u+1u]);
            result.push_back(source_indices[std::size_t(i)*3u+2u]);
        }else if(type==D3DPT_TRIANGLESTRIP) {
            result.push_back(source_indices[i+(i&1u)]);
            result.push_back(source_indices[i+1u-(i&1u)]);
            result.push_back(source_indices[i+2u]);
        }else if(type==D3DPT_TRIANGLEFAN) {
            result.push_back(source_indices[0u]);
            result.push_back(source_indices[i+1u]);
            result.push_back(source_indices[i+2u]);
        }
    }
    triangle_indices=std::move(result);
    return true;
}
} // namespace m2vr::drawprobe
