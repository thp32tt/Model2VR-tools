#pragma once
// Source-state admission for the verified D3D9 fixed-function texture path.
// Only POINT/LINEAR + CLAMP/WRAP/MIRROR are representable.
// Mipmap, sRGB sampling, texture-coordinate transforms and unmodelled
// alpha compares must never be silently approximated in stereo output.
#include <d3d9.h>
#include "../vr/model2_texture_sampler.hpp"
#include "../vr/model2_raster_states.hpp"
#include "../vr/model2_blend_states.hpp"
#include <cstdint>

namespace m2vr::drawprobe {
// One-level original D3D9 TEXTURE resources have no other mip to
// interpolate or select. POINT/LINEAR MIPFILTER is therefore equivalent
// to NONE, but only after querying the exact bound original resource.
// The query is read-only and always releases its COM reference.
inline bool OriginalStage0TextureHasOneMip(IDirect3DDevice9* d) noexcept {
    if(!d)return false;
    IDirect3DBaseTexture9* source=nullptr;
    if(FAILED(d->GetTexture(0,&source)) || !source){
        if(source)source->Release();
        return false;
    }
    const bool single=source->GetType()==D3DRTYPE_TEXTURE &&
                      source->GetLevelCount()==1u;
    source->Release();
    return single;
}
inline bool DecodeSamplerPolicy(DWORD address_u,DWORD address_v,
                                DWORD min_filter,DWORD mag_filter,
                                DWORD mip_filter,DWORD srgb_texture,
                                m2vr::vr::SamplerPolicy& out,
                                bool original_single_mip=false) noexcept {
    using m2vr::vr::TextureAddress;
    using m2vr::vr::TextureFilter;
    if((address_u!=D3DTADDRESS_CLAMP && address_u!=D3DTADDRESS_WRAP &&
        address_u!=D3DTADDRESS_MIRROR &&
        address_u!=D3DTADDRESS_MIRRORONCE) ||
       (address_v!=D3DTADDRESS_CLAMP && address_v!=D3DTADDRESS_WRAP &&
        address_v!=D3DTADDRESS_MIRROR &&
        address_v!=D3DTADDRESS_MIRRORONCE) ||
       (min_filter!=D3DTEXF_POINT && min_filter!=D3DTEXF_LINEAR) ||
       (mag_filter!=D3DTEXF_POINT && mag_filter!=D3DTEXF_LINEAR) ||
       min_filter!=mag_filter ||
       (mip_filter!=D3DTEXF_NONE &&
        !(original_single_mip &&
          (mip_filter==D3DTEXF_POINT ||
           mip_filter==D3DTEXF_LINEAR))) ||
       srgb_texture!=0u)return false;
    m2vr::vr::SamplerPolicy value{};
    value.u=address_u==D3DTADDRESS_WRAP ? TextureAddress::Wrap:
            address_u==D3DTADDRESS_MIRROR ? TextureAddress::Mirror:
            address_u==D3DTADDRESS_MIRRORONCE ? TextureAddress::MirrorOnce:
                                            TextureAddress::Clamp;
    value.v=address_v==D3DTADDRESS_WRAP ? TextureAddress::Wrap:
            address_v==D3DTADDRESS_MIRROR ? TextureAddress::Mirror:
            address_v==D3DTADDRESS_MIRRORONCE ? TextureAddress::MirrorOnce:
                                            TextureAddress::Clamp;
    value.filter=min_filter==D3DTEXF_LINEAR ? TextureFilter::Linear:
                                               TextureFilter::Point;
    out=value;
    return true;
}
inline bool DecodeRasterPolicy(DWORD cull_mode,DWORD zfunc,
                               DWORD zwrite,DWORD stencil_enabled,
                               DWORD depth_bias,DWORD slope_bias,
                               DWORD srgb_write,DWORD color_mask,
                               m2vr::vr::RasterPolicy& out) noexcept {
    using m2vr::vr::FaceCull;
    using m2vr::vr::DepthCompare;
    if((cull_mode!=D3DCULL_NONE && cull_mode!=D3DCULL_CW &&
        cull_mode!=D3DCULL_CCW) ||
       (zfunc!=D3DCMP_LESS && zfunc!=D3DCMP_LESSEQUAL &&
        zfunc!=D3DCMP_ALWAYS && zfunc!=D3DCMP_EQUAL &&
        zfunc!=D3DCMP_GREATER && zfunc!=D3DCMP_GREATEREQUAL &&
        zfunc!=D3DCMP_NOTEQUAL) ||
       (zwrite!=0u && zwrite!=1u) ||
       stencil_enabled!=0u || depth_bias!=0u || slope_bias!=0u ||
       srgb_write!=0u ||
       (color_mask&~(D3DCOLORWRITEENABLE_RED|
                     D3DCOLORWRITEENABLE_GREEN|
                     D3DCOLORWRITEENABLE_BLUE|
                     D3DCOLORWRITEENABLE_ALPHA))!=0u)
        return false;
    m2vr::vr::RasterPolicy value{};
    value.cull=cull_mode==D3DCULL_CW?FaceCull::Clockwise:
               cull_mode==D3DCULL_CCW?FaceCull::CounterClockwise:
                                        FaceCull::None;
    // Preserve original ZFUNC across both independently clipped eye Z buffers.
    value.depth=zfunc==D3DCMP_LESS?DepthCompare::Less:
                zfunc==D3DCMP_LESSEQUAL?DepthCompare::LessEqual:
                zfunc==D3DCMP_ALWAYS?DepthCompare::Always:
                zfunc==D3DCMP_EQUAL?DepthCompare::Equal:
                zfunc==D3DCMP_GREATER?DepthCompare::Greater:
                zfunc==D3DCMP_GREATEREQUAL?DepthCompare::GreaterEqual:
                                           DepthCompare::NotEqual;
    value.write_depth=(zwrite!=0u);
    value.write_rgba_mask=static_cast<std::uint8_t>(color_mask);
    out=value;
    return true;
}
// Original depth must share the physical game color target's dimensions
// and sample layout. A depth-disabled HUD/auxiliary surface must never be
// promoted into a calibrated world pass merely by its viewport.
inline bool CompatibleOriginalDepthSurface(
    const D3DSURFACE_DESC& color,
    const D3DSURFACE_DESC& depth) noexcept {
    return color.Width>0u && color.Height>0u &&
        depth.Width==color.Width && depth.Height==color.Height &&
        depth.MultiSampleType==color.MultiSampleType &&
        depth.MultiSampleQuality==color.MultiSampleQuality &&
        (depth.Usage&D3DUSAGE_DEPTHSTENCIL)!=0u;
}
// D3D9 returns D3DERR_NOTFOUND for an unbound additional target.
// Other failures are UNKNOWN, not an assurance of single-target rendering.
inline bool EmptyExtraRenderTarget(HRESULT query, bool has_surface) noexcept {
    return query==D3DERR_NOTFOUND ||
           (SUCCEEDED(query) && !has_surface);
}
// The original D3D9 scissor ENABLE bit is not itself a crop: an exact
// full-backbuffer rectangle is semantically identical to scissor disabled.
// Any unknown rectangle, negative edge or partial crop remains unsupported.
inline bool FullBackbufferScissor(DWORD enabled,const RECT* rectangle,
                                  const D3DSURFACE_DESC& backbuffer) noexcept {
    if(enabled==0u)return true;
    return enabled==1u && rectangle &&
           backbuffer.Width>0u && backbuffer.Height>0u &&
           backbuffer.Width<=0x7fffffffu &&
           backbuffer.Height<=0x7fffffffu &&
           rectangle->left==0 && rectangle->top==0 &&
           rectangle->right>0 && rectangle->bottom>0 &&
           static_cast<std::uint32_t>(rectangle->right)==backbuffer.Width &&
           static_cast<std::uint32_t>(rectangle->bottom)==backbuffer.Height;
}
inline bool FullBackbufferViewport(DWORD scissor_enabled,
                                   const D3DVIEWPORT9& viewport,
                                   const D3DSURFACE_DESC& backbuffer,
                                   const RECT* scissor_rectangle=nullptr) noexcept {
    // World-eye raster cannot emulate a partial source viewport, scissor
    // crop or remapped source depth interval. Accept only a proven no-op
    // source scissor; never promote mirror/HUD/auxiliary sources.
    return FullBackbufferScissor(scissor_enabled,scissor_rectangle,backbuffer) &&
           backbuffer.Width>0u && backbuffer.Height>0u &&
           viewport.X==0u && viewport.Y==0u &&
           viewport.Width==backbuffer.Width &&
           viewport.Height==backbuffer.Height &&
           viewport.MinZ==0.f && viewport.MaxZ==1.f;
}
// Fail closed on unknown RGB/alpha equations: only non-separate ADD is
// exactly reproducible in the independent binocular CPU render targets.
inline bool DecodeBlendPolicy(DWORD enable,DWORD src,DWORD dst,
                              DWORD operation,DWORD separate,
                              m2vr::vr::BlendMode& out) noexcept {
    using m2vr::vr::BlendMode;
    if(enable==0u){out=BlendMode::Opaque;return true;}
    if(enable!=1u || separate!=0u || operation!=D3DBLENDOP_ADD)
        return false;
    if(src==D3DBLEND_ONE && dst==D3DBLEND_ZERO)
        out=BlendMode::ReplaceRGBA;
    else if(src==D3DBLEND_SRCALPHA && dst==D3DBLEND_INVSRCALPHA)
        out=BlendMode::SourceAlphaOver;
    else if(src==D3DBLEND_ONE && dst==D3DBLEND_ONE)
        out=BlendMode::Additive;
    else if(src==D3DBLEND_SRCALPHA && dst==D3DBLEND_ONE)
        out=BlendMode::SourceAlphaAdd;
    else if(src==D3DBLEND_ONE && dst==D3DBLEND_INVSRCALPHA)
        out=BlendMode::PremultipliedOver;
    else if((src==D3DBLEND_DESTCOLOR && dst==D3DBLEND_ZERO) ||
            (src==D3DBLEND_ZERO && dst==D3DBLEND_SRCCOLOR))
        out=BlendMode::MultiplyColor;
    else if(src==D3DBLEND_DESTALPHA && dst==D3DBLEND_INVDESTALPHA)
        out=BlendMode::DestinationAlphaOver;
    else if(src==D3DBLEND_ZERO && dst==D3DBLEND_ONE)
        out=BlendMode::PreserveDestination;
    else return false;
    return true;
}
inline bool AlphaCutoffMatches(DWORD alpha_enable,DWORD alpha_func,
                               DWORD alpha_ref,
                               unsigned int requested_cutoff) noexcept {
    if(requested_cutoff>255u)return false;
    // The stereo raster compares unsigned 8-bit alpha >= cutoff.
    // Admit only D3D9 comparisons mathematically equivalent for ALL
    // 256 input values; unsupported comparisons still fail closed.
    if(alpha_enable==0u)return requested_cutoff==0u;
    if(alpha_enable!=1u || alpha_ref>255u)return false;
    if(alpha_func==D3DCMP_GREATEREQUAL)
        return requested_cutoff==alpha_ref;
    if(alpha_func==D3DCMP_GREATER)
        return alpha_ref<255u && requested_cutoff==alpha_ref+1u;
    if(alpha_func==D3DCMP_ALWAYS ||
       (alpha_func==D3DCMP_LESSEQUAL && alpha_ref==255u))
        return requested_cutoff==0u;
    if(alpha_func==D3DCMP_NOTEQUAL && alpha_ref==0u)
        return requested_cutoff==1u;
    if(alpha_func==D3DCMP_EQUAL && alpha_ref==255u)
        return requested_cutoff==255u;
    return false;
}
} // namespace m2vr::drawprobe
