// Real D3D9 material -> existing Model2VR textured stereo bridge.
// Trust boundary: a separate *verified* game renderer adapter must already
// have begun a proven, calibrated world pass. This code never guesses it
// from FVF, never hooks automatically and never touches simulation or FFB.
#include "verified_world_bridge.h"
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
                           m2vr::vr::SamplerPolicy& sampler,
                           m2vr::vr::RasterPolicy& raster,
                           m2vr::drawprobe::FixedStageSource& rgb_source,
                           m2vr::drawprobe::FixedStageSource& alpha_source,
                           m2vr::vr::BlendMode& blend_mode) {
    if(!d)return false;
    IDirect3DVertexShader9* vs=nullptr;
    if(FAILED(d->GetVertexShader(&vs)))return false;
    if(vs){vs->Release();return false;}
    IDirect3DPixelShader9* ps=nullptr;
    if(FAILED(d->GetPixelShader(&ps)))return false;
    if(ps){ps->Release();return false;}
    IDirect3DBaseTexture9* second=nullptr;
    if(FAILED(d->GetTexture(1,&second)))return false;
    if(second){second->Release();return false;}
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
    if(FAILED(d->GetRenderState(D3DRS_ZENABLE,&z)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend_enable)) ||
       FAILED(d->GetRenderState(D3DRS_SRCBLEND,&blend_src)) ||
       FAILED(d->GetRenderState(D3DRS_DESTBLEND,&blend_dst)) ||
       FAILED(d->GetRenderState(D3DRS_BLENDOP,&blend_op)) ||
       FAILED(d->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,&separate_alpha)) ||
       FAILED(d->GetRenderState(D3DRS_FOGENABLE,&fog)) ||
       FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting)) ||
       FAILED(d->GetRenderState(D3DRS_SPECULARENABLE,&specular)) ||
       z!=D3DZB_TRUE || fog || lighting || specular ||
       !m2vr::drawprobe::DecodeBlendPolicy(
            blend_enable,blend_src,blend_dst,blend_op,separate_alpha,
            blend_mode))return false;
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
    if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&address_u)) ||
       FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&address_v)) ||
       FAILED(d->GetSamplerState(0,D3DSAMP_MINFILTER,&min_filter)) ||
       FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag_filter)) ||
       FAILED(d->GetSamplerState(0,D3DSAMP_MIPFILTER,&mip_filter)) ||
       FAILED(d->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb_texture)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha_test)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHAREF,&alpha_ref)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHAFUNC,&alpha_func)) ||
       FAILED(d->GetTextureStageState(1,D3DTSS_COLOROP,&stage1_colorop)) ||
       FAILED(d->GetTextureStageState(1,D3DTSS_ALPHAOP,&stage1_alphaop)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_RESULTARG,&stage0_result)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_TEXCOORDINDEX,&texcoord_index)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,
                                     &transform_flags)) ||
       stage1_colorop!=D3DTOP_DISABLE ||
       stage1_alphaop!=D3DTOP_DISABLE ||
       stage0_result!=D3DTA_CURRENT ||
       texcoord_index!=0u || transform_flags!=D3DTTFF_DISABLE ||
       (!m2vr::drawprobe::FixedStagePairIsDiffuseOnly(
            rgb_source,alpha_source) &&
        !m2vr::drawprobe::DecodeSamplerPolicy(address_u,address_v,
            min_filter,mag_filter,mip_filter,srgb_texture,sampler)) ||
       !m2vr::drawprobe::AlphaCutoffMatches(
            alpha_test,alpha_func,alpha_ref,cutoff))return false;
    IDirect3DSurface9* current=nullptr;
    IDirect3DSurface9* main=nullptr;
    const bool have_current=SUCCEEDED(d->GetRenderTarget(0,&current))&&current;
    const bool have_main=SUCCEEDED(d->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&main))&&main;
    const bool same=have_current&&have_main&&current==main;
    D3DSURFACE_DESC backbuffer_desc{};
    D3DVIEWPORT9 viewport{};
    DWORD scissor_enabled=1u;
    const bool full_view=same &&
        SUCCEEDED(main->GetDesc(&backbuffer_desc)) &&
        SUCCEEDED(d->GetViewport(&viewport)) &&
        SUCCEEDED(d->GetRenderState(D3DRS_SCISSORTESTENABLE,
                                    &scissor_enabled)) &&
        m2vr::drawprobe::FullBackbufferViewport(
            scissor_enabled,viewport,backbuffer_desc);
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
    auto* d=static_cast<IDirect3DDevice9*>(device);
    m2vr::vr::SamplerPolicy sampler{};
    m2vr::vr::RasterPolicy raster{};
    m2vr::drawprobe::FixedStageSource rgb_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::drawprobe::FixedStageSource alpha_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::vr::BlendMode blend_mode=m2vr::vr::BlendMode::Opaque;
    if(!d || !Model2VR_IsOriginalPrimaryD3D9Device(d) ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       !source || !triangle_indices ||
       !count || count>2048 || !index_count ||
       index_count%3 || index_count>6144u || alpha_cutoff>255u)
        return FALSE;
    DWORD fvf=0;
    if(FAILED(d->GetFVF(&fvf)))return FALSE;
    const auto layout=m2vr::drawprobe::DecodeWorldUpFVF(fvf,stride);
    // D3D9 missing FVF DIFFUSE has an exact source-defined value:
    // opaque white 0xffffffff, already produced by DecodeWorldVertex.
    // Stage0 CURRENT is identical to DIFFUSE; neither requires guessed data.
    if(!layout.eligible ||
       !FixedFunctionMaterial(d,alpha_cutoff,sampler,raster,
                              rgb_source,alpha_source,blend_mode))
        return FALSE;
    D3DMATRIX world{},view{},projection{};
    if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) ||
       FAILED(d->GetTransform(D3DTS_VIEW,&view)) ||
       FAILED(d->GetTransform(D3DTS_PROJECTION,&projection)) ||
       !m2vr::drawprobe::AffineMatrix(world) ||
       !m2vr::drawprobe::AffineMatrix(view) ||
       !m2vr::drawprobe::StandardPerspectiveProjection(projection))
        return FALSE;
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
                      safe_indices.size()*sizeof(std::uint32_t)))return FALSE;
        for(const auto index:safe_indices)if(index>=count)return FALSE;
        for(unsigned int i=0;i<count;++i){
            m2vr::drawprobe::WorldVertex vertex{};
            if(!m2vr::drawprobe::DecodeWorldVertex(
                  raw.data()+std::size_t(i)*stride,stride,layout,
                  world,view,vertex))return FALSE;
            for(unsigned k=0;k<3;++k)
                positions[std::size_t(i)*3u+k]=vertex.view_xyz[k];
            for(unsigned k=0;k<2;++k)
                uv[std::size_t(i)*2u+k]=vertex.uv[k];
            if(!diffuse.empty())diffuse[i]=vertex.diffuse_argb;
        }
        // When BOTH D3D9 stage-zero channels select actual vertex DIFFUSE,
        // bound texture contents are semantically unused. Feed the existing
        // calibrated two-eye raster a harmless 1x1 staging placeholder;
        // neither RGB nor alpha can sample it in this admitted material.
        if(m2vr::drawprobe::FixedStagePairIsDiffuseOnly(
            rgb_source,alpha_source)){
            texture.width=1u;texture.height=1u;
            texture.rgba.assign(4u,255u);
        }else if(!m2vr::drawprobe::SnapshotCurrentTexture(d,texture))
            return FALSE;
    }catch(...){return FALSE;}
    // No second haptic owner, no duplicated emulator CPU draw or state.
    return Model2VR_AppendVerifiedTexturedTrianglesWithMaterialSourceRenderStates(
        positions.data(),uv.data(),count,safe_indices.data(),index_count,
        texture.rgba.data(),texture.width,texture.height,texture.width*4u,
        alpha_cutoff,
        static_cast<unsigned int>(sampler.filter),
        static_cast<unsigned int>(sampler.u),
        static_cast<unsigned int>(sampler.v),
        static_cast<unsigned int>(raster.cull),
        static_cast<unsigned int>(raster.depth),
        raster.write_depth?1u:0u,
        diffuse.empty()?nullptr:diffuse.data(),
        static_cast<unsigned>(rgb_source),
        static_cast<unsigned>(alpha_source),
        static_cast<unsigned>(blend_mode));
}

// Actual original x86 D3D9 DrawPrimitiveUP XYZRHW/TEX1 vertices may already
// match the same-thread ProjectVertexToScreen source ring. With a separately
// authenticated CPU-world transaction, reconstruct ONLY these proven
// camera-space positions, never rewrite source 2D or call original Draw twice.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9UPTextured(
    void* device,const void* original_vertices,unsigned int triangles,
    unsigned int stride,unsigned int expected_cpu_caller,unsigned int cutoff) {
    auto* d=static_cast<IDirect3DDevice9*>(device);
    constexpr std::uint32_t kMaxTriangles=
        static_cast<std::uint32_t>(
            m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles);
    if(!d || !Model2VR_IsOriginalPrimaryD3D9Device(d) ||
       !original_vertices || !expected_cpu_caller ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       triangles<2u || triangles>kMaxTriangles || cutoff>255u)return FALSE;
    m2vr::vr::SamplerPolicy sampler{};
    m2vr::vr::RasterPolicy raster{};
    m2vr::drawprobe::FixedStageSource rgb_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::drawprobe::FixedStageSource alpha_source=
        m2vr::drawprobe::FixedStageSource::Texture;
    m2vr::vr::BlendMode blend_mode=m2vr::vr::BlendMode::Opaque;
    // Original projected D3D9 FVF may carry a packed D3DCOLOR before UV.
    // When source FVF omits DIFFUSE, D3D9 defines opaque-white default
    // rather than an unknown attribute. We preserve exactly that default.
    DWORD fvf=0;
    if(FAILED(d->GetFVF(&fvf)) ||
       !m2vr::drawprobe::ExactProjectedUPWorldFVF(fvf,stride) ||
       !FixedFunctionMaterial(d,cutoff,sampler,raster,
                              rgb_source,alpha_source,blend_mode))return FALSE;
    D3DVIEWPORT9 viewport{};
    if(FAILED(d->GetViewport(&viewport)))return FALSE;
    try {
        const auto count=std::size_t(triangles)*3u;
        std::vector<std::uint8_t> raw(count*stride);
        if(!GuardCopy(raw.data(),original_vertices,raw.size()))return FALSE;
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
                raw.data()+i*stride,fvf,stride,decoded))return FALSE;
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
            return FALSE;
        const auto fit=m2vr::drawprobe::FitSourcePerspective(
            joined.camera_samples.data(),joined.camera_sample_count,
            viewport.X,viewport.Y,viewport.Width,viewport.Height);
        if(!fit.plausible)return FALSE;
        for(std::size_t i=0;i<count;++i)
            for(unsigned k=0;k<3u;++k)
                positions[i*3u+k]=joined.camera_samples[i].source_xyz[k];
        m2vr::drawprobe::TextureSnapshot texture{};
        // When BOTH D3D9 stage-zero channels select actual vertex DIFFUSE,
        // bound texture contents are semantically unused. Feed the existing
        // calibrated two-eye raster a harmless 1x1 staging placeholder;
        // neither RGB nor alpha can sample it in this admitted material.
        if(m2vr::drawprobe::FixedStagePairIsDiffuseOnly(
            rgb_source,alpha_source)){
            texture.width=1u;texture.height=1u;
            texture.rgba.assign(4u,255u);
        }else if(!m2vr::drawprobe::SnapshotCurrentTexture(d,texture))
            return FALSE;
        // Same authenticated owner and exact existing rasterization/pose
        // contract as the other fixed-function original Draw adapters.
        return Model2VR_AppendVerifiedTexturedTrianglesWithMaterialSourceRenderStates(
            positions.data(),uv.data(),static_cast<unsigned>(count),
            indices.data(),static_cast<unsigned>(count),
            texture.rgba.data(),texture.width,texture.height,
            texture.width*4u,cutoff,
            static_cast<unsigned>(sampler.filter),
            static_cast<unsigned>(sampler.u),
            static_cast<unsigned>(sampler.v),
            static_cast<unsigned>(raster.cull),
            static_cast<unsigned>(raster.depth),
            raster.write_depth?1u:0u,
            diffuse.empty()?nullptr:diffuse.data(),
            static_cast<unsigned>(rgb_source),
        static_cast<unsigned>(alpha_source),
            static_cast<unsigned>(blend_mode));
    }catch(...){return FALSE;}
}
