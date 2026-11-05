#pragma once
// Both authentic original x86 D3D9 transform COM routes feed one source-Draw
// nonce. No matrix contents or pointers; no automatic World3D permission.
#include "original_d3d9_lineage.hpp"
#include <cstdint>
namespace m2vr::drawprobe {
enum class OriginalTransformCategory : std::uint32_t {
    View=1u, Projection=2u, World=3u, Texture=4u, Other=5u
};
// D3DTS_VIEW=2; PROJECTION=3; TEXTURE0..7=16..23; WORLD0..255=256..511.
constexpr OriginalTransformCategory ClassifyOriginalTransform(
    std::uint32_t state) noexcept {
    if(state==2u)return OriginalTransformCategory::View;
    if(state==3u)return OriginalTransformCategory::Projection;
    if(state>=256u && state<=511u)return OriginalTransformCategory::World;
    if(state>=16u && state<=23u)return OriginalTransformCategory::Texture;
    return OriginalTransformCategory::Other;
}
constexpr OriginalLineageKind OriginalTransformLineageKind(
    OriginalTransformCategory kind) noexcept {
    switch(kind){
    case OriginalTransformCategory::View:return OriginalLineageKind::View;
    case OriginalTransformCategory::Projection:return OriginalLineageKind::Projection;
    case OriginalTransformCategory::World:return OriginalLineageKind::World;
    case OriginalTransformCategory::Texture:return OriginalLineageKind::TextureTransform;
    default:return OriginalLineageKind::OtherTransform;
    }
}
} // namespace m2vr::drawprobe
