#pragma once
// Strict, read-only decoder for the exact D3D9 UP declaration observed
// in the user's 2026-11-03 Daytona 2D trace. This is NOT world/HUD proof.
#include <d3d9.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace m2vr::drawprobe {
inline bool ExactDaytonaUPDeclaration(const D3DVERTEXELEMENT9* e,
                                     UINT count, UINT stride) noexcept {
    if(!e || count!=5u || stride!=32u)return false;
    const auto matches=[](const D3DVERTEXELEMENT9& a,
                          WORD stream,WORD offset,BYTE type,
                          BYTE usage,BYTE index) noexcept {
        return a.Stream==stream && a.Offset==offset &&
               a.Type==type && a.Method==D3DDECLMETHOD_DEFAULT &&
               a.Usage==usage && a.UsageIndex==index;
    };
    return matches(e[0],0u,0u,D3DDECLTYPE_FLOAT4,
                   D3DDECLUSAGE_POSITION,0u) &&
           matches(e[1],0u,16u,D3DDECLTYPE_D3DCOLOR,
                   D3DDECLUSAGE_COLOR,0u) &&
           matches(e[2],0u,20u,D3DDECLTYPE_D3DCOLOR,
                   D3DDECLUSAGE_COLOR,1u) &&
           matches(e[3],0u,24u,D3DDECLTYPE_FLOAT2,
                   D3DDECLUSAGE_TEXCOORD,0u) &&
           matches(e[4],0xffu,0u,D3DDECLTYPE_UNUSED,0u,0u);
}
struct DaytonaUPVertex {
    std::array<float,4> position{}; // source FLOAT4, not assumed 3D
    std::array<float,2> texcoord{};
};
inline bool DecodeDaytonaUPVertex(
    const std::array<std::uint8_t,32>& bytes,
    DaytonaUPVertex& output) noexcept {
    DaytonaUPVertex candidate{};
    std::memcpy(candidate.position.data(),bytes.data(),16u);
    std::memcpy(candidate.texcoord.data(),bytes.data()+24u,8u);
    for(const auto v:candidate.position)if(!std::isfinite(v))return false;
    for(const auto v:candidate.texcoord)if(!std::isfinite(v))return false;
    output=candidate;
    return true;
}
} // namespace m2vr::drawprobe
