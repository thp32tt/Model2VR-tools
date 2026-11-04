// Real D3D9 material -> existing Model2VR textured stereo bridge.
// Trust boundary: a separate *verified* game renderer adapter must already
// have begun a proven, calibrated world pass. This code never guesses it
// from FVF, never hooks automatically and never touches simulation or FFB.
#include "verified_world_bridge.h"
#include "verified_world_source_result.hpp"
#include "d3d9_texture_snapshot.hpp"
#include "d3d9_sampler_states.hpp"
#include "d3d9_fixed_stage_policy.hpp"
#include "project_draw_correlator.hpp"
#include "projected_up_world_source.hpp"
#include "model2_camera_projection_fit.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
bool GuardCopy(void* dst,const void* src,std::size_t size) noexcept {
    if(!dst||!src||!size||size>512u*1024u)return false;
    __try { std::memcpy(dst,src,size);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool NoAdditionalColorTargets(IDirect3DDevice9* d) {
    D3DCAPS9 caps{};
    if(!d || FAILED(d->GetDeviceCaps(&caps)) ||
       caps.NumSimultaneousRTs<1u || caps.NumSimultaneousRTs>4u)
        return false;
    for(DWORD slot=1u;slot<caps.NumSimultaneousRTs;++slot){
        IDirect3DSurface9* extra=nullptr;
        const HRESULT result=d->GetRenderTarget(slot,&extra);
        const bool empty=m2vr::drawprobe::EmptyExtraRenderTarget(
            result,extra!=nullptr);
        if(extra)extra->Release();
        if(!empty)return false;
    }
    return true;
}
bool FixedFunctionMaterial(IDirect3DDevice9* d,unsigned cutoff,
                           DWORD source_fvf,
                           m2vr::vr::SamplerPolicy& sampler,
                           m2vr::vr::RasterPolicy& raster,
                           m2vr::drawprobe::FixedStageSource& rgb_source,
                           m2vr::drawprobe::FixedStageSource& alpha_source,
                           m2vr::vr::BlendMode& blend_mode,
                           unsigned int& alpha_compare,
                           unsigned int& alpha_reference,
                           m2vr::vr::SeparateAlphaBlend& separate_policy) {
    if(!d)return false;
    IDirect3DVertexShader9* vs=nullptr;
    if(FAILED(d->GetVertexShader(&vs)))return false;
    if(vs){vs->Release();return false;}
    IDirect3DPixelShader9* ps=nullptr;
    if(FAILED(d->GetPixelShader(&ps)))return false;
    if(ps){ps->Release();return false;}
    // The original may leave texture slot1 bound after an earlier draw.
    // Both stage1 operations are required DISABLED below, so the binding
    // is inert and must not disqualify an otherwise equivalent world pass.
    // Require original fixed-function vertex shading and clip behavior to
    // match the independent per-eye Gouraud/5-plane raster. The source
    // World/View matrix alone cannot reproduce GPU vertex blending or user
    // clip planes. Query the ACTUAL original Draw device; fail closed.
    DWORD shade=0,clipping=0,clip_planes=0,vertex_blend=0,indexed_blend=0,fill=0;
    if(FAILED(d->GetRenderState(D3DRS_FILLMODE,&fill)) ||
       !m2vr::drawprobe::OriginalFilledTriangleEquivalent(fill) ||
       FAILED(d->GetRenderState(D3DRS_SHADEMODE,&shade)) ||
       FAILED(d->GetRenderState(D3DRS_CLIPPING,&clipping)) ||
       FAILED(d->GetRenderState(D3DRS_CLIPPLANEENABLE,&clip_planes)) ||
       FAILED(d->GetRenderState(D3DRS_VERTEXBLEND,&vertex_blend)) ||
       FAILED(d->GetRenderState(
           D3DRS_INDEXEDVERTEXBLENDENABLE,&indexed_blend)) ||
       !m2vr::drawprobe::OriginalVertexPipelineEquivalent(
           shade,clipping,clip_planes,vertex_blend,indexed_blend))
        return false;
    DWORD zfunc=0,zwrite=0,cull=0,stencil=0,dbias=0,sbias=0;
    DWORD srgb_write=0,color_mask=0;
    if(FAILED(d->GetRenderState(D3DRS_ZFUNC,&zfunc)) ||
       FAILED(d->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite)) ||
       FAILED(d->GetRenderState(D3DRS_CULLMODE,&cull)) ||
       FAILED(d->GetRenderState(D3DRS_STENCILENABLE,&stencil)) ||
       FAILED(d->GetRenderState(D3DRS_DEPTHBIAS,&dbias)) ||
       FAILED(d->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&sbias)) ||
       FAILED(d->GetRenderState(D3DRS_SRGBWRITEENABLE,&srgb_write)) ||
       FAILED(d->GetRenderState(D3DRS_COLORWRITEENABLE,&color_mask)) ||
       !m2vr::drawprobe::DecodeRasterPolicy(cull,zfunc,zwrite,
          stencil,dbias,sbias,srgb_write,color_mask,raster))
        return false;
    DWORD z=0,blend_enable=1,fog=1,lighting=1,specular=1;
    DWORD blend_src=0,blend_dst=0,blend_op=0,separate_alpha=1;
    DWORD alpha_blend_src=0,alpha_blend_dst=0,alpha_blend_op=0;
    if(FAILED(d->GetRenderState(D3DRS_ZENABLE,&z)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend_enable)) ||
       FAILED(d->GetRenderState(D3DRS_SRCBLEND,&blend_src)) ||
       FAILED(d->GetRenderState(D3DRS_DESTBLEND,&blend_dst)) ||
       FAILED(d->GetRenderState(D3DRS_BLENDOP,&blend_op)) ||
       FAILED(d->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,&separate_alpha)) ||
       FAILED(d->GetRenderState(D3DRS_FOGENABLE,&fog)) ||
       FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting)) ||
       FAILED(d->GetRenderState(D3DRS_SPECULARENABLE,&specular)) ||
       z!=D3DZB_TRUE || fog || specular ||
       !m2vr::drawprobe::FixedStageOriginalLightingEquivalent(
           lighting,source_fvf) ||
       (blend_enable==1u && separate_alpha==1u &&
        (color_mask&D3DCOLORWRITEENABLE_ALPHA)!=0u &&
        (FAILED(d->GetRenderState(D3DRS_SRCBLENDALPHA,&alpha_blend_src)) ||
         FAILED(d->GetRenderState(D3DRS_DESTBLENDALPHA,&alpha_blend_dst)) ||
         FAILED(d->GetRenderState(D3DRS_BLENDOPALPHA,&alpha_blend_op)))) ||
       !m2vr::drawprobe::DecodeMaskedSeparateAlphaPolicy(
            blend_enable,blend_src,blend_dst,blend_op,separate_alpha,
            alpha_blend_src,alpha_blend_dst,alpha_blend_op,color_mask,
            blend_mode,separate_policy))return false;
    DWORD colorop=0,arg=0,arg2=0,alphaop=0,alphaarg=0,alphaarg2=0;
    if(FAILED(d->GetTextureStageState(0,D3DTSS_COLOROP,&colorop)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_COLORARG1,&arg)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_COLORARG2,&arg2)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAOP,&alphaop)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAARG1,&alphaarg)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAARG2,&alphaarg2)))
        return false;
    if(!m2vr::drawprobe::DecodeFixedStageSource(
           colorop,arg,arg2,rgb_source) ||
       !m2vr::drawprobe::DecodeFixedStageSource(
           alphaop,alphaarg,alphaarg2,alpha_source))
        return false;
    DWORD address_u=0,address_v=0,min_filter=0,mag_filter=0,mip_filter=0;
    DWORD srgb_texture=0,alpha_test=0,alpha_ref=0,alpha_func=0;
    DWORD stage1_colorop=0,stage1_alphaop=0,stage0_result=0;
    DWORD texcoord_index=0,transform_flags=0;
    if(FAILED(d->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha_test)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHAREF,&alpha_ref)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHAFUNC,&alpha_func)) ||
       FAILED(d->GetTextureStageState(1,D3DTSS_COLOROP,&stage1_colorop)) ||
       FAILED(d->GetTextureStageState(1,D3DTSS_ALPHAOP,&stage1_alphaop)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_RESULTARG,&stage0_result)) ||
       !m2vr::drawprobe::FixedStageOneDisabled(
           stage1_colorop,stage1_alphaop) ||
       stage0_result!=D3DTA_CURRENT ||
       !m2vr::drawprobe::DecodeOriginalAlphaCompare(
            alpha_test,alpha_func,alpha_ref,cutoff,
            alpha_compare,alpha_reference))return false;
    // An original diffuse-only TEX0 Draw cannot sample coordinates or
    // sampler states. Avoid six irrelevant COM queries and never require
    // UV transforms on an input stream without UV. Texture-dependent
    // operations continue to be validated against exact original state.
    if(m2vr::drawprobe::FixedStagePairUsesTexture(
            rgb_source,alpha_source) &&
       (FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&address_u)) ||
        FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&address_v)) ||
        FAILED(d->GetSamplerState(0,D3DSAMP_MINFILTER,&min_filter)) ||
        FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag_filter)) ||
        FAILED(d->GetSamplerState(0,D3DSAMP_MIPFILTER,&mip_filter)) ||
        FAILED(d->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb_texture)) ||
        FAILED(d->GetTextureStageState(
            0,D3DTSS_TEXCOORDINDEX,&texcoord_index)) ||
        FAILED(d->GetTextureStageState(
            0,D3DTSS_TEXTURETRANSFORMFLAGS,&transform_flags)) ||
        texcoord_index!=0u || transform_flags!=D3DTTFF_DISABLE ||
        !m2vr::drawprobe::DecodeSamplerPolicy(
            address_u,address_v,min_filter,mag_filter,mip_filter,
            srgb_texture,sampler,
            mip_filter!=D3DTEXF_NONE &&
                m2vr::drawprobe::OriginalStage0TextureHasOneMip(d))))
        return false;
    IDirect3DSurface9* current=nullptr;
    IDirect3DSurface9* main=nullptr;
    const bool have_current=SUCCEEDED(d->GetRenderTarget(0,&current))&&current;
    const bool have_main=SUCCEEDED(d->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&main))&&main;
    const bool same=have_current&&have_main&&current==main;
    D3DSURFACE_DESC backbuffer_desc{};
    D3DVIEWPORT9 viewport{};
    DWORD scissor_enabled=1u;
    RECT scissor_rectangle{};
    const bool full_view=same &&
        SUCCEEDED(main->GetDesc(&backbuffer_desc)) &&
        SUCCEEDED(d->GetViewport(&viewport)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_SCISSORTESTENABLE,
                                    &scissor_enabled)) &&
        (scissor_enabled==0u ||
         (scissor_enabled==1u &&
          SUCCEEDED(d->GetScissorRect(&scissor_rectangle)))) &&
        m2vr::drawprobe::FullBackbufferViewport(
            scissor_enabled,viewport,backbuffer_desc,
            scissor_enabled ? &scissor_rectangle : nullptr);
    if(current)current->Release();
    if(main)main->Release();
    if(!full_view || !NoAdditionalColorTargets(d))return false;
    IDirect3DSurface9* depth=nullptr;
    if(FAILED(d->GetDepthStencilSurface(&depth)) || !depth){
        if(depth)depth->Release();
        return false;
    }
    D3DSURFACE_DESC depth_desc{};
    const bool depth_compatible=SUCCEEDED(depth->GetDesc(&depth_desc)) &&
        m2vr::drawprobe::CompatibleOriginalDepthSurface(
            backbuffer_desc,depth_desc);
    depth->Release();
    return depth_compatible;
}
} // namespace

extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9UPTextured(
    void* device, const void* source, unsigned int count,
    unsigned int stride, const std::uint32_t* triangle_indices,
    unsigned int index_count,unsigned int alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
    auto* d=static_cast<IDirect3DDevice9*>(device);
    m2vr::vr::SamplerPolicy sampler{};
    m2vr::vr::RasterPolicy raster{};
    m2vr::drawprobe::FixedStageSource rgb_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::drawprobe::FixedStageSource alpha_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::vr::BlendMode blend_mode=m2vr::vr::BlendMode::Opaque;
    m2vr::vr::SeparateAlphaBlend separate_policy{};
    unsigned int alpha_compare=0u,alpha_reference=0u;
    if(!d || !Model2VR_IsOriginalPrimaryD3D9Device(d) ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       !source || !triangle_indices ||
       !count || count>2048 || !index_count ||
       index_count%3 || index_count>6144u || alpha_cutoff>256u)
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
    DWORD fvf=0;
    if(FAILED(d->GetFVF(&fvf)))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
    const auto layout=m2vr::drawprobe::DecodeWorldUpFVF(fvf,stride);
    // D3D9 missing FVF DIFFUSE has an exact source-defined value:
    // opaque white 0xffffffff, already produced by DecodeWorldVertex.
    // Stage0 CURRENT is identical to DIFFUSE; neither requires guessed data.
    if(!layout.eligible ||
       !FixedFunctionMaterial(d,alpha_cutoff,fvf,sampler,raster,
                              rgb_source,alpha_source,blend_mode,
                              alpha_compare,alpha_reference,
                              separate_policy) ||
       !m2vr::drawprobe::FixedStageUVAvailableForSources(
           layout.has_uv,rgb_source,alpha_source))
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
    D3DMATRIX world{},view{},projection{};
    if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) ||
       FAILED(d->GetTransform(D3DTS_VIEW,&view)) ||
       FAILED(d->GetTransform(D3DTS_PROJECTION,&projection)) ||
       !m2vr::drawprobe::AffineMatrix(world) ||
       !m2vr::drawprobe::AffineMatrix(view) ||
       !m2vr::drawprobe::StandardPerspectiveProjection(projection))
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
    std::vector<std::uint8_t> raw;
    std::vector<std::uint32_t> safe_indices;
    std::vector<float> positions,uv;
    std::vector<std::uint32_t> diffuse;
    m2vr::drawprobe::TextureSnapshot texture{};
    try {
        raw.resize(std::size_t(count)*stride);
        safe_indices.resize(index_count);
        positions.resize(std::size_t(count)*3u);
        uv.resize(std::size_t(count)*2u);
        if(rgb_source!=m2vr::drawprobe::FixedStageSource::Texture ||
           alpha_source!=m2vr::drawprobe::FixedStageSource::Texture)
            diffuse.resize(count);
        if(!GuardCopy(raw.data(),source,raw.size()) ||
           !GuardCopy(safe_indices.data(),triangle_indices,
                      safe_indices.size()*sizeof(std::uint32_t)))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
        for(const auto index:safe_indices)if(index>=count)return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
        for(unsigned int i=0;i<count;++i){
            m2vr::drawprobe::WorldVertex vertex{};
            if(!m2vr::drawprobe::DecodeWorldVertex(
                  raw.data()+std::size_t(i)*stride,stride,layout,
                  world,view,vertex))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
            for(unsigned k=0;k<3;++k)
                positions[std::size_t(i)*3u+k]=vertex.view_xyz[k];
            for(unsigned k=0;k<2;++k)
                uv[std::size_t(i)*2u+k]=vertex.uv[k];
            if(!diffuse.empty())diffuse[i]=vertex.diffuse_argb;
        }
        // No dummy 1x1 texel: original D3D9 TEX0/DIFFUSE never samples
        // stage-zero texture when both RGB and alpha select DIFFUSE.
        if(!m2vr::drawprobe::FixedStagePairIsDiffuseOnly(
                rgb_source,alpha_source) &&
           !m2vr::drawprobe::SnapshotCurrentTexture(d,texture))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);}
    // No second haptic owner, no duplicated emulator CPU draw or state.
    return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates(
        positions.data(),uv.data(),count,safe_indices.data(),index_count,
        texture.rgba.data(),texture.width,texture.height,texture.width*4u,
        alpha_cutoff==256u?0u:alpha_cutoff,
        static_cast<unsigned int>(sampler.filter),
        static_cast<unsigned int>(sampler.u),
        static_cast<unsigned int>(sampler.v),
        static_cast<unsigned int>(raster.cull),
        static_cast<unsigned int>(raster.depth),
        raster.write_depth?1u:0u,
        diffuse.empty()?nullptr:diffuse.data(),
        static_cast<unsigned>(rgb_source),
        static_cast<unsigned>(alpha_source),
        static_cast<unsigned>(blend_mode),
        static_cast<unsigned>(raster.write_rgba_mask),
        alpha_compare,alpha_reference,
        separate_policy.enabled?1u:0u,
        static_cast<unsigned>(separate_policy.source),
        static_cast<unsigned>(separate_policy.destination))),
        m2vr::vrbridge::OriginalSourceDrawKind::PrimitiveUP);
}

// Actual original x86 D3D9 DrawPrimitiveUP XYZRHW/TEX1 vertices may already
// match the same-thread ProjectVertexToScreen source ring. With a separately
// authenticated CPU-world transaction, reconstruct ONLY these proven
// camera-space positions, never rewrite source 2D or call original Draw twice.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9UPTextured(
    void* device,const void* original_vertices,unsigned int triangles,
    unsigned int stride,unsigned int expected_cpu_caller,unsigned int cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
    auto* d=static_cast<IDirect3DDevice9*>(device);
    constexpr std::uint32_t kMaxTriangles=
        static_cast<std::uint32_t>(
            m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles);
    if(!d || !Model2VR_IsOriginalPrimaryD3D9Device(d) ||
       !original_vertices || !expected_cpu_caller ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       triangles<2u || triangles>kMaxTriangles || cutoff>256u)return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
    m2vr::vr::SamplerPolicy sampler{};
    m2vr::vr::RasterPolicy raster{};
    m2vr::drawprobe::FixedStageSource rgb_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::drawprobe::FixedStageSource alpha_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::vr::BlendMode blend_mode=m2vr::vr::BlendMode::Opaque;
    m2vr::vr::SeparateAlphaBlend separate_policy{};
    unsigned int alpha_compare=0u,alpha_reference=0u;
    // Original projected D3D9 FVF may carry a packed D3DCOLOR before UV.
    // When source FVF omits DIFFUSE, D3D9 defines opaque-white default
    // rather than an unknown attribute. We preserve exactly that default.
    DWORD fvf=0;
    if(FAILED(d->GetFVF(&fvf)) ||
       !m2vr::drawprobe::ExactProjectedUPWorldFVF(fvf,stride) ||
       !FixedFunctionMaterial(d,cutoff,fvf,sampler,raster,
                              rgb_source,alpha_source,blend_mode,
                              alpha_compare,alpha_reference,
                              separate_policy) ||
       !m2vr::drawprobe::FixedStageUVAvailableForSources(
           (fvf&D3DFVF_TEXCOUNT_MASK)==D3DFVF_TEX1,
           rgb_source,alpha_source))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
    D3DVIEWPORT9 viewport{};
    if(FAILED(d->GetViewport(&viewport)))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
    try {
        const auto count=std::size_t(triangles)*3u;
        std::vector<std::uint8_t> raw(count*stride);
        if(!GuardCopy(raw.data(),original_vertices,raw.size()))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
        std::array<m2vr::drawprobe::ProjectedDrawVertex,24> screen{};
        std::vector<float> uv(count*2u),positions(count*3u);
        std::vector<std::uint32_t> indices(count);
        std::vector<std::uint32_t> diffuse;
        if(rgb_source!=m2vr::drawprobe::FixedStageSource::Texture ||
           alpha_source!=m2vr::drawprobe::FixedStageSource::Texture)
            diffuse.resize(count);
        for(std::size_t i=0;i<count;++i){
            m2vr::drawprobe::ProjectedUPWorldVertex decoded{};
            if(!m2vr::drawprobe::DecodeProjectedUPWorldVertex(
                raw.data()+i*stride,fvf,stride,decoded))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
            screen[i]={decoded.screen_xy[0],decoded.screen_xy[1]};
            uv[i*2u]=decoded.uv[0];
            uv[i*2u+1u]=decoded.uv[1];
            if(!diffuse.empty())
                diffuse[i]=decoded.diffuse_argb; // exact FVF default white
            indices[i]=static_cast<std::uint32_t>(i);
        }
        const auto joined=m2vr::drawprobe::ThreadProjectDrawCorrelator()
            .MatchTriangleList(m2vr::drawprobe::CurrentProjectDrawEpoch(),
                               screen.data(),count);
        if(!joined.StableOriginalProjectionProvenance() ||
           joined.common_source_caller_rva!=expected_cpu_caller ||
           !Model2VR_OriginalCpuProjectionStillMatchesDraw(
               joined.common_matrix_identity,
               joined.common_matrix_fingerprint,
               joined.common_geometry_command))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
        const auto fit=m2vr::drawprobe::FitSourcePerspective(
            joined.camera_samples.data(),joined.camera_sample_count,
            viewport.X,viewport.Y,viewport.Width,viewport.Height);
        if(!fit.plausible)return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
        for(std::size_t i=0;i<count;++i)
            for(unsigned k=0;k<3u;++k)
                positions[i*3u+k]=joined.camera_samples[i].source_xyz[k];
        m2vr::drawprobe::TextureSnapshot texture{};
        // No dummy 1x1 texel: original D3D9 TEX0/DIFFUSE never samples
        // stage-zero texture when both RGB and alpha select DIFFUSE.
        if(!m2vr::drawprobe::FixedStagePairIsDiffuseOnly(
                rgb_source,alpha_source) &&
           !m2vr::drawprobe::SnapshotCurrentTexture(d,texture))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
        // Same authenticated owner and exact existing rasterization/pose
        // contract as the other fixed-function original Draw adapters.
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates(
            positions.data(),uv.data(),static_cast<unsigned>(count),
            indices.data(),static_cast<unsigned>(count),
            texture.rgba.data(),texture.width,texture.height,
            texture.width*4u,cutoff==256u?0u:cutoff,
            static_cast<unsigned>(sampler.filter),
            static_cast<unsigned>(sampler.u),
            static_cast<unsigned>(sampler.v),
            static_cast<unsigned>(raster.cull),
            static_cast<unsigned>(raster.depth),
            raster.write_depth?1u:0u,
            diffuse.empty()?nullptr:diffuse.data(),
            static_cast<unsigned>(rgb_source),
        static_cast<unsigned>(alpha_source),
            static_cast<unsigned>(blend_mode),
            static_cast<unsigned>(raster.write_rgba_mask),
        alpha_compare,alpha_reference,
        separate_policy.enabled?1u:0u,
        static_cast<unsigned>(separate_policy.source),
        static_cast<unsigned>(separate_policy.destination))),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedPrimitiveUP);}
}
