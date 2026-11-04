#pragma once
// Actual D3D9 render-state gate for the exact known Daytona shader-UP
// caller/vertex declaration. It does not independently prove world ownership.
#include <d3d9.h>
#include <array>
#include <cstddef>
#include <cstdint>
namespace m2vr::daytona {
enum class ShaderCandidateReject : std::uint32_t {
    None=0, DeviceQuery=1, Viewport=2, Depth=3, BlendAlpha=4,
    Stencil=5, Scissor=6, WriteMask=7, Bias=8, ClipPlane=9,
    Cull=10, Srgb=11
};
struct ShaderCandidateState {
    DWORD z_enable=0,z_write=0,z_func=0,alpha_blend=0,alpha_test=0;
    DWORD stencil=0,scissor=0,color_mask=0,depth_bias=0,slope_bias=0;
    DWORD clip_plane_enable=0,cull=0,srgb_write=0;
};
inline ShaderCandidateReject ClassifyShaderUPCandidate(
    const ShaderCandidateState& s,const D3DVIEWPORT9& vp,
    UINT width,UINT height) noexcept {
    // Off-axis views, menu subrects and depth remapping need independently
    // verified camera calibration. Never assign a shader-X eye shift here.
    if(!width || !height ||
       vp.X!=0 || vp.Y!=0 || vp.Width!=width || vp.Height!=height ||
       vp.MinZ!=0.f || vp.MaxZ!=1.f)
        return ShaderCandidateReject::Viewport;
    if(s.z_enable!=D3DZB_TRUE || s.z_write!=TRUE ||
       (s.z_func!=D3DCMP_LESS && s.z_func!=D3DCMP_LESSEQUAL))
        return ShaderCandidateReject::Depth;
    if(s.alpha_blend!=FALSE || s.alpha_test!=FALSE)
        return ShaderCandidateReject::BlendAlpha;
    if(s.stencil!=FALSE)return ShaderCandidateReject::Stencil;
    if(s.scissor!=FALSE)return ShaderCandidateReject::Scissor;
    constexpr DWORD kRgba=D3DCOLORWRITEENABLE_RED|
                            D3DCOLORWRITEENABLE_GREEN|
                            D3DCOLORWRITEENABLE_BLUE|
                            D3DCOLORWRITEENABLE_ALPHA;
    if(s.color_mask!=kRgba)return ShaderCandidateReject::WriteMask;
    if(s.depth_bias!=0u || s.slope_bias!=0u)
        return ShaderCandidateReject::Bias;
    if(s.clip_plane_enable!=0u)return ShaderCandidateReject::ClipPlane;
    if(s.cull!=D3DCULL_NONE && s.cull!=D3DCULL_CW &&
       s.cull!=D3DCULL_CCW)return ShaderCandidateReject::Cull;
    if(s.srgb_write!=FALSE)return ShaderCandidateReject::Srgb;
    return ShaderCandidateReject::None;
}
inline ShaderCandidateReject CheckShaderUPCandidate(
    IDirect3DDevice9* device,const D3DVIEWPORT9& viewport,
    UINT width,UINT height) noexcept {
    if(!device)return ShaderCandidateReject::DeviceQuery;
    ShaderCandidateState s{};
    struct Register {D3DRENDERSTATETYPE index;DWORD* dst;};
    const std::array<Register,13> states{{
        {D3DRS_ZENABLE,&s.z_enable},
        {D3DRS_ZWRITEENABLE,&s.z_write},
        {D3DRS_ZFUNC,&s.z_func},
        {D3DRS_ALPHABLENDENABLE,&s.alpha_blend},
        {D3DRS_ALPHATESTENABLE,&s.alpha_test},
        {D3DRS_STENCILENABLE,&s.stencil},
        {D3DRS_SCISSORTESTENABLE,&s.scissor},
        {D3DRS_COLORWRITEENABLE,&s.color_mask},
        {D3DRS_DEPTHBIAS,&s.depth_bias},
        {D3DRS_SLOPESCALEDEPTHBIAS,&s.slope_bias},
        {D3DRS_CLIPPLANEENABLE,&s.clip_plane_enable},
        {D3DRS_CULLMODE,&s.cull},
        {D3DRS_SRGBWRITEENABLE,&s.srgb_write}
    }};
    for(const auto& item:states)
        if(FAILED(device->GetRenderState(item.index,item.dst)))
            return ShaderCandidateReject::DeviceQuery;
    return ClassifyShaderUPCandidate(s,viewport,width,height);
}
constexpr std::size_t kShaderCandidateReasons=12;
} // namespace m2vr::daytona
