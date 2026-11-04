#pragma once
// Strict fixed-function DrawPrimitiveUP world/material layout decoder.
// This module never claims world proof from FVF alone, never mutates D3D9
// state and never reconstructs view space from XYZRHW screen coordinates.
#include <d3d9.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace m2vr::drawprobe {
struct WorldUpLayout {
    std::uint32_t stride=0;
    std::uint32_t uv_offset=0;
    std::uint32_t diffuse_offset=0;
    bool has_diffuse=false;
    bool eligible=false;
};
inline WorldUpLayout DecodeWorldUpFVF(std::uint32_t fvf,
                                      std::uint32_t stride) noexcept {
    WorldUpLayout result{};
    if((fvf&D3DFVF_POSITION_MASK)!=D3DFVF_XYZ ||
       (fvf&D3DFVF_TEXCOUNT_MASK)!=D3DFVF_TEX1 ||
       (fvf&0xffff0000u)!=0u)return result; // TEXCOORDSIZE2 only
    constexpr std::uint32_t allowed=D3DFVF_XYZ|D3DFVF_NORMAL|
        D3DFVF_PSIZE|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1;
    if((fvf&~allowed)!=0)return result;
    std::uint32_t offset=12;
    if(fvf&D3DFVF_NORMAL)offset+=12;
    if(fvf&D3DFVF_PSIZE)offset+=4;
    if(fvf&D3DFVF_DIFFUSE){
        result.diffuse_offset=offset;
        result.has_diffuse=true;
        offset+=4;
    }
    if(fvf&D3DFVF_SPECULAR)offset+=4;
    // Fail on unknown packed attributes, padding or non-2D UV variants.
    if(stride!=offset+8u || stride>256u)return result;
    result.stride=stride;
    result.uv_offset=offset;
    result.eligible=true;
    return result;
}
struct WorldVertex {
    float view_xyz[3]{};
    float uv[2]{};
    // Original D3DCOLOR is A8R8G8B8, not little-endian RGBA.
    std::uint32_t diffuse_argb=0xffffffffu;
};
inline bool FiniteVertex(const WorldVertex& v) noexcept {
    for(float x:v.view_xyz)if(!std::isfinite(x))return false;
    for(float x:v.uv)if(!std::isfinite(x)||std::abs(x)>65536.f)return false;
    return true;
}
inline bool AffineMatrix(const D3DMATRIX& m) noexcept {
    const float row[16]={
        m._11,m._12,m._13,m._14,m._21,m._22,m._23,m._24,
        m._31,m._32,m._33,m._34,m._41,m._42,m._43,m._44
    };
    for(float v:row)if(!std::isfinite(v))return false;
    return std::abs(m._14)<0.00001f && std::abs(m._24)<0.00001f &&
           std::abs(m._34)<0.00001f && std::abs(m._44-1.f)<0.00001f;
}
// The original D3D9 projection must be a conventional homogeneous
// perspective transform before an XYZ world stream is independently sent to
// an XR asymmetric projection. FVF XYZ alone also occurs in menu/2D passes.
// D3D row-vector perspective has w'=+z (LH) or -z (RH), unlike ortho w'=1.
// Permit off-axis projection terms _31/_32, reject unproven shear/oblique,
// reversed-Z and non-standard matrix forms rather than label them 3D world.
inline bool StandardPerspectiveProjection(const D3DMATRIX& m) noexcept {
    const float all[16]={
        m._11,m._12,m._13,m._14,m._21,m._22,m._23,m._24,
        m._31,m._32,m._33,m._34,m._41,m._42,m._43,m._44
    };
    for(float v:all)if(!std::isfinite(v))return false;
    constexpr float epsilon=0.00001f;
    const bool left_handed=std::abs(m._34-1.f)<0.001f;
    const bool right_handed=std::abs(m._34+1.f)<0.001f;
    return (left_handed||right_handed) &&
           m._11>epsilon && m._22>epsilon &&
           std::abs(m._12)<epsilon && std::abs(m._13)<epsilon &&
           std::abs(m._14)<epsilon && std::abs(m._21)<epsilon &&
           std::abs(m._23)<epsilon && std::abs(m._24)<epsilon &&
           std::abs(m._41)<epsilon && std::abs(m._42)<epsilon &&
           std::abs(m._44)<epsilon && m._43< -epsilon &&
           m._33*m._34>epsilon;
}
inline void TransformPoint(const D3DMATRIX& m,const float source[3],
                           float destination[3]) noexcept {
    const double x=source[0],y=source[1],z=source[2];
    destination[0]=static_cast<float>(x*m._11+y*m._21+z*m._31+m._41);
    destination[1]=static_cast<float>(x*m._12+y*m._22+z*m._32+m._42);
    destination[2]=static_cast<float>(x*m._13+y*m._23+z*m._33+m._43);
}
inline bool DecodeWorldVertex(const void* data,std::size_t bytes,
                              const WorldUpLayout& layout,
                              const D3DMATRIX& world,const D3DMATRIX& view,
                              WorldVertex& output) noexcept {
    if(!data || !layout.eligible || bytes<layout.stride ||
       !AffineMatrix(world)||!AffineMatrix(view))return false;
    float xyz[3]{},uv[2]{};
    std::memcpy(xyz,data,sizeof(xyz));
    std::memcpy(uv,static_cast<const std::uint8_t*>(data)+layout.uv_offset,
                sizeof(uv));
    float w[3]{};
    TransformPoint(world,xyz,w);
    TransformPoint(view,w,output.view_xyz);
    output.uv[0]=uv[0];output.uv[1]=uv[1];
    if(layout.has_diffuse)
        std::memcpy(&output.diffuse_argb,
            static_cast<const std::uint8_t*>(data)+layout.diffuse_offset,
            sizeof(output.diffuse_argb));
    else
        output.diffuse_argb=0xffffffffu;
    return FiniteVertex(output);
}
// CPU-lockable 32-bit original D3D9 texel formats. Integer 10-bit formats
// are quantized to the RGBA8 staging IPC before independent-eye sampling.
inline constexpr bool SupportedTrueColor32(D3DFORMAT format) noexcept {
    return format==D3DFMT_A8R8G8B8 || format==D3DFMT_X8R8G8B8 ||
        format==D3DFMT_A8B8G8R8 || format==D3DFMT_X8B8G8R8 ||
        format==D3DFMT_A2R10G10B10 || format==D3DFMT_A2B10G10R10;
}
inline constexpr std::uint8_t Quantize10To8(std::uint32_t v) noexcept {
    return static_cast<std::uint8_t>((v*255u+511u)/1023u);
}
// Decode original little-endian memory order, not the symbolic enum order.
inline bool DecodeTextureRowRGBA(const std::uint8_t* src,
                                 std::uint8_t* dst,
                                 std::uint32_t width,
                                 D3DFORMAT format) noexcept {
    if(!src || !dst || !width || width>1024u ||
       !SupportedTrueColor32(format))return false;
    for(std::uint32_t x=0;x<width;++x){
        const auto i=std::size_t(x)*4u;
        if(format==D3DFMT_A8B8G8R8 || format==D3DFMT_X8B8G8R8){
            dst[i]=src[i];dst[i+1u]=src[i+1u];dst[i+2u]=src[i+2u];
            dst[i+3u]=format==D3DFMT_X8B8G8R8?255u:src[i+3u];
        }else if(format==D3DFMT_A2R10G10B10 ||
                 format==D3DFMT_A2B10G10R10){
            const std::uint32_t packed=std::uint32_t(src[i]) |
                (std::uint32_t(src[i+1u])<<8u) |
                (std::uint32_t(src[i+2u])<<16u) |
                (std::uint32_t(src[i+3u])<<24u);
            const std::uint32_t low=packed&1023u;
            const std::uint32_t mid=(packed>>10u)&1023u;
            const std::uint32_t high=(packed>>20u)&1023u;
            dst[i]=Quantize10To8(
                format==D3DFMT_A2R10G10B10?high:low);
            dst[i+1u]=Quantize10To8(mid);
            dst[i+2u]=Quantize10To8(
                format==D3DFMT_A2R10G10B10?low:high);
            dst[i+3u]=static_cast<std::uint8_t>(
                ((packed>>30u)&3u)*85u);
        }else{
            dst[i]=src[i+2u];dst[i+1u]=src[i+1u];dst[i+2u]=src[i];
            dst[i+3u]=format==D3DFMT_X8R8G8B8?255u:src[i+3u];
        }
    }
    return true;
}
} // namespace m2vr::drawprobe
