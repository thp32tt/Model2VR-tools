#pragma once
// Actual primary original D3D9 source-state write lineage, never World3D
// proof. Stamps contain ONLY bounded counters, not source camera matrices,
// renderer pointers, vertices, ROM/game assets or original screen pixels.
#include <array>
#include <cstddef>
#include <cstdint>

namespace m2vr::drawprobe {
enum class OriginalLineageKind : std::uint32_t {
    View=0, Projection=1, World=2, VertexConstants=3,
    RenderTarget=4, DepthSurface=5, Viewport=6, Scissor=7,
    StreamSource=8, IndexBuffer=9, VertexDeclaration=10, VertexFVF=11,
    VertexShader=12, PixelShader=13,
    PixelConstants=14, TextureBinding=15, TextureStage=16,
    SamplerState=17, RenderState=18, StateBlockApply=19
};
inline constexpr std::size_t kOriginalLineageKinds=20;
inline constexpr std::uint32_t kMaxLineageWritesPerPresent=1000000u;
struct OriginalLineageStamp {
    std::uint64_t physical_present=0;
    std::array<std::uint32_t,kOriginalLineageKinds> successful_writes{};
};
// A camera *write* is required, but is not itself an authenticated gameplay
// camera. The verified adapter still requires exact physical Draw and XR pose.
constexpr bool OriginalLineageUsable(const OriginalLineageStamp& stamp) noexcept {
    if(stamp.physical_present==0 || (!stamp.successful_writes[0] &&
       !stamp.successful_writes[1] && !stamp.successful_writes[3]))return false;
    for(auto n:stamp.successful_writes)
        if(n>kMaxLineageWritesPerPresent)return false;
    return true;
}
constexpr bool SameOriginalLineage(const OriginalLineageStamp& lhs,
                                   const OriginalLineageStamp& rhs) noexcept {
    if(!OriginalLineageUsable(lhs)||!OriginalLineageUsable(rhs)||
       lhs.physical_present!=rhs.physical_present)return false;
    for(std::size_t i=0;i<kOriginalLineageKinds;++i)
        if(lhs.successful_writes[i]!=rhs.successful_writes[i])return false;
    return true;
}
// FNV1a over one Present ID and all camera/region/geometry/shader/material counters. A compact non-secret
// consistency fingerprint, NOT cryptographic or a World3D authorization.
constexpr std::uint64_t OriginalLineageNonce(
    const OriginalLineageStamp& stamp) noexcept {
    if(!OriginalLineageUsable(stamp))return 0;
    std::uint64_t hash=14695981039346656037ull;
    const auto append=[&hash](std::uint64_t v,unsigned bytes){
        for(unsigned i=0;i<bytes;++i){
            hash^=(v>>(i*8u))&0xffu;
            hash*=1099511628211ull;
        }
    };
    append(stamp.physical_present,8u);
    for(auto n:stamp.successful_writes)append(n,4u);
    return hash?hash:1u;
}
} // namespace m2vr::drawprobe
