// Explicit, verified-world-only source adapter for *actual* D3D9
// DrawPrimitive / DrawIndexedPrimitive vertex and index buffers.
// It neither hooks draws automatically nor publishes unproved stereo frames.
// Only CPU-readable, non-WRITEONLY FVF XYZ/TEX1 triangle lists are accepted.
#include "verified_world_bridge.h"
#include "d3d9_world_source.hpp"
#include "d3d9_vb_index_window.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
template<class T> struct ComOwner {
    T* ptr=nullptr;
    ~ComOwner(){if(ptr)ptr->Release();}
    ComOwner(const ComOwner&)=delete;
    ComOwner& operator=(const ComOwner&)=delete;
    ComOwner()=default;
};
template<class T> struct ReadLock {
    T* resource;
    bool locked=false;
    explicit ReadLock(T* p):resource(p){}
    ~ReadLock(){if(locked)resource->Unlock();}
    ReadLock(const ReadLock&)=delete;
    ReadLock& operator=(const ReadLock&)=delete;
    bool Acquire(UINT offset,UINT size,void** data){
        if(!resource || !data || !size)return false;
        locked=SUCCEEDED(resource->Lock(offset,size,data,D3DLOCK_READONLY));
        return locked && *data;
    }
};
bool GuardRead(void* dst,const void* src,std::size_t size) noexcept {
    if(!dst||!src||!size||size>512u*1024u)return false;
    __try {std::memcpy(dst,src,size);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

// A locked buffer is released BEFORE the existing UP material adapter takes
// the actual world/view/texture snapshot. No D3D9 device state is written.
bool CopyVBWindow(IDirect3DDevice9* device,
                  const m2vr::drawprobe::VBVertexWindow& window,
                  std::vector<std::uint8_t>& bytes,UINT& stride) {
    if(!device || !window.count || window.count>2048u)return false;
    ComOwner<IDirect3DVertexBuffer9> vb;
    UINT stream_offset=0;
    if(FAILED(device->GetStreamSource(
           0,&vb.ptr,&stream_offset,&stride)) || !vb.ptr)return false;
    DWORD fvf=0;
    if(FAILED(device->GetFVF(&fvf)) ||
       !m2vr::drawprobe::DecodeWorldUpFVF(fvf,stride).eligible)return false;
    D3DVERTEXBUFFER_DESC desc{};
    if(FAILED(vb.ptr->GetDesc(&desc)) ||
       (desc.Usage&D3DUSAGE_WRITEONLY))return false;
    const std::uint64_t offset=std::uint64_t(stream_offset)+
        std::uint64_t(window.first)*stride;
    const std::uint64_t size=std::uint64_t(window.count)*stride;
    if(!size || size>512u*1024u || offset>desc.Size ||
       size>std::uint64_t(desc.Size)-offset ||
       offset>UINT32_MAX)return false;
    bytes.resize(static_cast<std::size_t>(size));
    {
        ReadLock<IDirect3DVertexBuffer9> lock(vb.ptr);
        void* mapped=nullptr;
        if(!lock.Acquire(static_cast<UINT>(offset),
                         static_cast<UINT>(size),&mapped))return false;
        if(!GuardRead(bytes.data(),mapped,bytes.size()))return false;
    }
    return true;
}
bool ReadIndices(IDirect3DDevice9* device,UINT start_index,
                 UINT triangle_count,INT base_vertex,UINT min_vertex,
                 UINT num_vertices,
                 m2vr::drawprobe::VBVertexWindow& out) {
    if(!device || !triangle_count ||
       triangle_count>m2vr::drawprobe::kMaxVBBridgeTriangles)return false;
    ComOwner<IDirect3DIndexBuffer9> ib;
    if(FAILED(device->GetIndices(&ib.ptr)) || !ib.ptr)return false;
    D3DINDEXBUFFER_DESC desc{};
    if(FAILED(ib.ptr->GetDesc(&desc)) ||
       (desc.Usage&D3DUSAGE_WRITEONLY) ||
       (desc.Format!=D3DFMT_INDEX16 &&
        desc.Format!=D3DFMT_INDEX32))return false;
    const UINT index_width=desc.Format==D3DFMT_INDEX16?2u:4u;
    const std::uint64_t offset=std::uint64_t(start_index)*index_width;
    const std::uint64_t length=std::uint64_t(triangle_count)*3u*index_width;
    if(!length || offset>desc.Size ||
       length>std::uint64_t(desc.Size)-offset ||
       offset>UINT32_MAX || length>UINT32_MAX)return false;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    {
        ReadLock<IDirect3DIndexBuffer9> lock(ib.ptr);
        void* mapped=nullptr;
        if(!lock.Acquire(static_cast<UINT>(offset),
                         static_cast<UINT>(length),&mapped))return false;
        if(!GuardRead(data.data(),mapped,data.size()))return false;
    }
    return m2vr::drawprobe::RemapIndexedTriangleList(
        data.data(),data.size(),desc.Format,base_vertex,min_vertex,
        num_vertices,triangle_count,out);
}
BOOL SubmitBufferWindow(IDirect3DDevice9* device,
                        const m2vr::drawprobe::VBVertexWindow& window,
                        UINT alpha_cutoff) {
    if(!device || window.indices.empty() || alpha_cutoff>255u)
        return FALSE;
    try {
        std::vector<std::uint8_t> vertices;
        UINT stride=0;
        if(!CopyVBWindow(device,window,vertices,stride))return FALSE;
        // The established adapter rechecks the actual fixed-function
        // D3D9 material, texture, world/view matrices, proof-owned frame
        // and all buffer bounds before appending either eye.
        return Model2VR_AppendVerifiedD3D9UPTextured(
            device,vertices.data(),window.count,stride,
            window.indices.data(),
            static_cast<UINT>(window.indices.size()),alpha_cutoff);
    }catch(...){return FALSE;}
}
} // namespace

// Mirrors DrawPrimitive's StartVertex and PrimitiveCount; TRIANGLELIST only.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9VBTextured(
    void* device,unsigned int start_vertex,
    unsigned int primitive_count,unsigned int alpha_cutoff) {
    try {
        m2vr::drawprobe::VBVertexWindow window{};
        if(!m2vr::drawprobe::RemapSequentialTriangleList(
                start_vertex,primitive_count,window))return FALSE;
        return SubmitBufferWindow(static_cast<IDirect3DDevice9*>(device),
                                  window,alpha_cutoff);
    }catch(...){return FALSE;}
}

// Mirrors DrawIndexedPrimitive's BaseVertexIndex/MinVertexIndex/
// NumVertices/StartIndex/PrimitiveCount; TRIANGLELIST only.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedVBTextured(
    void* device,int base_vertex_index,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int start_index,unsigned int primitive_count,
    unsigned int alpha_cutoff) {
    try {
        m2vr::drawprobe::VBVertexWindow window{};
        if(!ReadIndices(static_cast<IDirect3DDevice9*>(device),
            start_index,primitive_count,base_vertex_index,min_vertex_index,
            num_vertices,window))return FALSE;
        return SubmitBufferWindow(static_cast<IDirect3DDevice9*>(device),
                                  window,alpha_cutoff);
    }catch(...){return FALSE;}
}
