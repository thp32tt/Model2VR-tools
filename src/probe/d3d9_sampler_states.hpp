#pragma once
// Source-state admission for the verified D3D9 fixed-function texture path.
// Only representable POINT/LINEAR + CLAMP/WRAP combinations are permitted.
// Mipmap, sRGB sampling, texture-coordinate transforms and unmodelled
// alpha compares must never be silently approximated in stereo output.
#include <d3d9.h>
#include "../vr/model2_texture_sampler.hpp"
#include <cstdint>

namespace m2vr::drawprobe {
inline bool DecodeSamplerPolicy(DWORD address_u,DWORD address_v,
                                DWORD min_filter,DWORD mag_filter,
                                DWORD mip_filter,DWORD srgb_texture,
                                m2vr::vr::SamplerPolicy& out) noexcept {
    using m2vr::vr::TextureAddress;
    using m2vr::vr::TextureFilter;
    if((address_u!=D3DTADDRESS_CLAMP && address_u!=D3DTADDRESS_WRAP) ||
       (address_v!=D3DTADDRESS_CLAMP && address_v!=D3DTADDRESS_WRAP) ||
       (min_filter!=D3DTEXF_POINT && min_filter!=D3DTEXF_LINEAR) ||
       (mag_filter!=D3DTEXF_POINT && mag_filter!=D3DTEXF_LINEAR) ||
       min_filter!=mag_filter ||
       mip_filter!=D3DTEXF_NONE || srgb_texture!=0u)return false;
    m2vr::vr::SamplerPolicy value{};
    value.u=address_u==D3DTADDRESS_WRAP ? TextureAddress::Wrap:
                                             TextureAddress::Clamp;
    value.v=address_v==D3DTADDRESS_WRAP ? TextureAddress::Wrap:
                                             TextureAddress::Clamp;
    value.filter=min_filter==D3DTEXF_LINEAR ? TextureFilter::Linear:
                                               TextureFilter::Point;
    out=value;
    return true;
}
inline bool AlphaCutoffMatches(DWORD alpha_enable,DWORD alpha_func,
                               DWORD alpha_ref,
                               unsigned int requested_cutoff) noexcept {
    if(requested_cutoff>255u)return false;
    // If alpha test is disabled, 0 ensures even texel alpha 0 survives.
    if(alpha_enable==0u)return requested_cutoff==0u;
    // Renderer implements >= and not >, ==, !=, etc.
    return alpha_enable==1u && alpha_func==D3DCMP_GREATEREQUAL &&
           alpha_ref<=255u && requested_cutoff==alpha_ref;
}
} // namespace m2vr::drawprobe
