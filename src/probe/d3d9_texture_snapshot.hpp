#pragma once
// Read-only D3D9 fixed-function material snapshot. No shader/texture writes,
// device reset, GPU copies, resource creation or speculative world labeling.
// LockRect can only read supported CPU-lockable original texture resources.
// DEFAULT-pool inaccessible textures fail closed instead of inventing pixels.
#include "d3d9_world_source.hpp"
#include "d3d9_dxt_decode.hpp"
#include "d3d9_rgb16_decode.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace m2vr::drawprobe {
struct TextureSnapshot {
    std::vector<std::uint8_t> rgba;
    std::uint32_t width=0,height=0;
    D3DFORMAT format=D3DFMT_UNKNOWN;
};
inline bool SafeCopyRow(void* dst,const void* src,std::size_t bytes) noexcept {
    if(!dst||!src||!bytes||bytes>4096u)return false;
    __try {
        std::memcpy(dst,src,bytes);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
inline bool SnapshotCurrentTexture(IDirect3DDevice9* dev,
                                   TextureSnapshot& out) {
    if(!dev)return false;
    IDirect3DBaseTexture9* base=nullptr;
    if(FAILED(dev->GetTexture(0,&base)) || !base)return false;
    // COM reference returned by GetTexture must always be released.
    struct ReleaseBase {
        IDirect3DBaseTexture9* value;
        ~ReleaseBase(){if(value)value->Release();}
    } owner{base};
    if(base->GetType()!=D3DRTYPE_TEXTURE)return false;
    auto* tex=static_cast<IDirect3DTexture9*>(base);
    D3DSURFACE_DESC desc{};
    if(FAILED(tex->GetLevelDesc(0,&desc)) ||
       !desc.Width || !desc.Height ||
       desc.Width>1024u || desc.Height>1024u ||
       (desc.Format!=D3DFMT_A8R8G8B8 &&
        desc.Format!=D3DFMT_X8R8G8B8 &&
        !DxtBytesPerBlock(desc.Format) &&
        !Supported16BitColor(desc.Format)))return false;
    D3DLOCKED_RECT locked{};
    if(FAILED(tex->LockRect(0,&locked,nullptr,D3DLOCK_READONLY)))
        return false;
    struct Unlock {
        IDirect3DTexture9* tex;
        ~Unlock(){if(tex)tex->UnlockRect(0);}
    } unlock{tex};
    const std::uint32_t dxt_bytes=DxtBytesPerBlock(desc.Format);
    const std::uint32_t row_bytes=dxt_bytes?
        ((desc.Width+3u)/4u)*dxt_bytes :
        desc.Width*(Supported16BitColor(desc.Format)?2u:4u);
    if(!locked.pBits || locked.Pitch<0 ||
       std::uint32_t(locked.Pitch)<row_bytes)return false;
    TextureSnapshot temp{};
    temp.width=desc.Width;
    temp.height=desc.Height;
    temp.format=desc.Format;
    try {
        temp.rgba.resize(std::size_t(desc.Width)*desc.Height*4u);
    }catch(...){return false;}
    std::array<std::uint8_t,4096> row{};
    const std::uint32_t rows=dxt_bytes?
        (desc.Height+3u)/4u:desc.Height;
    for(std::uint32_t y=0;y<rows;++y){
        const auto* origin=static_cast<const std::uint8_t*>(locked.pBits);
        if(!SafeCopyRow(row.data(),
                       origin+std::size_t(y)*std::size_t(locked.Pitch),
                       row_bytes))return false;
        if(dxt_bytes){
            const auto blocks=(desc.Width+3u)/4u;
            for(std::uint32_t x=0;x<blocks;++x){
                if(!DecodeDxtBlockRGBA(
                    row.data()+std::size_t(x)*dxt_bytes,
                    desc.Format,x,y,temp.rgba.data(),
                    desc.Width,desc.Height,desc.Width*4u))return false;
            }
        }else if(Supported16BitColor(desc.Format)){
            if(!Decode16BitColorRowRGBA(
                row.data(),temp.rgba.data()+std::size_t(y)*desc.Width*4u,
                desc.Width,desc.Format))return false;
        }else{
            if(!DecodeTextureRowRGBA(
                    row.data(),temp.rgba.data()+std::size_t(y)*desc.Width*4u,
                    desc.Width,desc.Format))return false;
        }
    }
    out=std::move(temp);
    return true;
}
} // namespace m2vr::drawprobe
