// Real D3D9 material -> existing Model2VR textured stereo bridge.
// Trust boundary: a separate *verified* game renderer adapter must already
// have begun a proven, calibrated world pass. This code never guesses it
// from FVF, never hooks automatically and never touches simulation or FFB.
#include "verified_world_bridge.h"
#include "d3d9_texture_snapshot.hpp"
#include "d3d9_sampler_states.hpp"
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
bool FixedFunctionMaterial(IDirect3DDevice9* d,unsigned cutoff,
                           m2vr::vr::SamplerPolicy& sampler,
                           m2vr::vr::RasterPolicy& raster) {
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
    DWORD z=0,blend=1,fog=1,lighting=1;
    if(FAILED(d->GetRenderState(D3DRS_ZENABLE,&z)) ||
       FAILED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend)) ||
       FAILED(d->GetRenderState(D3DRS_FOGENABLE,&fog)) ||
       FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting)) ||
       z!=D3DZB_TRUE || blend || fog || lighting)return false;
    DWORD colorop=0,arg=0,alphaop=0,alphaarg=0;
    if(FAILED(d->GetTextureStageState(0,D3DTSS_COLOROP,&colorop)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_COLORARG1,&arg)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAOP,&alphaop)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAARG1,&alphaarg)) ||
       colorop!=D3DTOP_SELECTARG1 || arg!=D3DTA_TEXTURE ||
       alphaop!=D3DTOP_SELECTARG1 || alphaarg!=D3DTA_TEXTURE)return false;
    DWORD address_u=0,address_v=0,min_filter=0,mag_filter=0,mip_filter=0;
    DWORD srgb_texture=0,alpha_test=0,alpha_ref=0,alpha_func=0;
    DWORD stage1_colorop=0,texcoord_index=0,transform_flags=0;
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
       FAILED(d->GetTextureStageState(0,D3DTSS_TEXCOORDINDEX,&texcoord_index)) ||
       FAILED(d->GetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,
                                     &transform_flags)) ||
       stage1_colorop!=D3DTOP_DISABLE ||
       texcoord_index!=0u || transform_flags!=D3DTTFF_DISABLE ||
       !m2vr::drawprobe::DecodeSamplerPolicy(address_u,address_v,
            min_filter,mag_filter,mip_filter,srgb_texture,sampler) ||
       !m2vr::drawprobe::AlphaCutoffMatches(
            alpha_test,alpha_func,alpha_ref,cutoff))return false;
    IDirect3DSurface9* current=nullptr;
    IDirect3DSurface9* main=nullptr;
    const bool have_current=SUCCEEDED(d->GetRenderTarget(0,&current))&&current;
    const bool have_main=SUCCEEDED(d->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&main))&&main;
    const bool same=have_current&&have_main&&current==main;
    if(current)current->Release();
    if(main)main->Release();
    return same;
}
} // namespace

extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9UPTextured(
    void* device, const void* source, unsigned int count,
    unsigned int stride, const std::uint32_t* triangle_indices,
    unsigned int index_count,unsigned int alpha_cutoff) {
    auto* d=static_cast<IDirect3DDevice9*>(device);
    m2vr::vr::SamplerPolicy sampler{};
    m2vr::vr::RasterPolicy raster{};
    if(!d || !source || !triangle_indices ||
       !count || count>2048 || !index_count ||
       index_count%3 || index_count>6144u || alpha_cutoff>255u ||
       !FixedFunctionMaterial(d,alpha_cutoff,sampler,raster))return FALSE;
    DWORD fvf=0;
    if(FAILED(d->GetFVF(&fvf)))return FALSE;
    const auto layout=m2vr::drawprobe::DecodeWorldUpFVF(fvf,stride);
    if(!layout.eligible)return FALSE;
    D3DMATRIX world{},view{};
    if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) ||
       FAILED(d->GetTransform(D3DTS_VIEW,&view)) ||
       !m2vr::drawprobe::AffineMatrix(world) ||
       !m2vr::drawprobe::AffineMatrix(view))return FALSE;
    std::vector<std::uint8_t> raw;
    std::vector<std::uint32_t> safe_indices;
    std::vector<float> positions,uv;
    m2vr::drawprobe::TextureSnapshot texture{};
    try {
        raw.resize(std::size_t(count)*stride);
        safe_indices.resize(index_count);
        positions.resize(std::size_t(count)*3u);
        uv.resize(std::size_t(count)*2u);
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
        }
        if(!m2vr::drawprobe::SnapshotCurrentTexture(d,texture))
            return FALSE;
    }catch(...){return FALSE;}
    // No second haptic owner, no duplicated emulator CPU draw or state.
    return Model2VR_AppendVerifiedTexturedTrianglesWithRenderStates(
        positions.data(),uv.data(),count,safe_indices.data(),index_count,
        texture.rgba.data(),texture.width,texture.height,texture.width*4u,
        alpha_cutoff,
        static_cast<unsigned int>(sampler.filter),
        static_cast<unsigned int>(sampler.u),
        static_cast<unsigned int>(sampler.v),
        static_cast<unsigned int>(raster.cull),
        static_cast<unsigned int>(raster.depth),
        raster.write_depth?1u:0u);
}
