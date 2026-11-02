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
    if(fvf&D3DFVF_DIFFUSE)offset+=4;
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
    return FiniteVertex(output);
}
// D3D9 A8R8G8B8 and X8R8G8B8 CPU-row color unpack (little-endian BGRA).
inline bool DecodeTextureRowRGBA(const std::uint8_t* src,
                                 std::uint8_t* dst,
                                 std::uint32_t width,
                                 D3DFORMAT format) noexcept {
    if(!src || !dst || !width || width>1024u ||
       (format!=D3DFMT_A8R8G8B8 && format!=D3DFMT_X8R8G8B8))
        return false;
    for(std::uint32_t x=0;x<width;++x){
        const auto i=std::size_t(x)*4;
        dst[i]=src[i+2];
        dst[i+1]=src[i+1];
        dst[i+2]=src[i];
        dst[i+3]=format==D3DFMT_X8R8G8B8?255:src[i+3];
    }
    return true;
}
} // namespace m2vr::drawprobe
