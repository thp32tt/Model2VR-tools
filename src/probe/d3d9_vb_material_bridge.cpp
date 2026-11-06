// Explicit, verified-world-only source adapter for *actual* D3D9
// DrawPrimitive / DrawIndexedPrimitive vertex and index buffers.
// It neither hooks draws automatically nor publishes unproved stereo frames.
// CPU-readable FVF XYZ/TEX1 lists, strips and fans are eligible only
// inside an independently authenticated world transaction.
#include "verified_world_bridge.h"
#include "verified_world_source_result.hpp"
#include "d3d9_world_source.hpp"
#include "projected_up_world_source.hpp"
#include "project_draw_correlator.hpp"
static_assert(m2vr::drawprobe::kMaxAuthenticatedProjectedCorners ==
    m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles*3u,
    "original projected VB/IB native eye expansion must match CPU ring cap");
#include "d3d9_projected_vb_observer.hpp"
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
                  std::vector<std::uint8_t>& bytes,UINT& stride,
                  bool projected_screen=false) {
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) ||
       !window.count || window.count>2048u)return false;
    ComOwner<IDirect3DVertexBuffer9> vb;
    UINT stream_offset=0;
    if(FAILED(device->GetStreamSource(
           0,&vb.ptr,&stream_offset,&stride)) || !vb.ptr)return false;
    DWORD fvf=0;
    if(FAILED(device->GetFVF(&fvf)) ||
       !(projected_screen ?
         m2vr::drawprobe::ExactProjectedUPWorldFVF(fvf,stride) :
         m2vr::drawprobe::DecodeWorldUpFVF(fvf,stride).eligible))
        return false;
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
bool ReadIndicesTopology(IDirect3DDevice9* device,
                 D3DPRIMITIVETYPE topology,UINT start_index,
                 UINT triangle_count,INT base_vertex,UINT min_vertex,
                 UINT num_vertices,
                 m2vr::drawprobe::VBVertexWindow& out) {
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) ||
       !triangle_count ||
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
    const auto element_count=m2vr::drawprobe::PrimitiveSourceElementCount(
        topology,triangle_count);
    const std::uint64_t length=std::uint64_t(element_count)*index_width;
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
    return m2vr::drawprobe::RemapIndexedTopology(
        data.data(),data.size(),desc.Format,topology,base_vertex,min_vertex,
        num_vertices,triangle_count,out);
}
// Existing verified TRIANGLELIST callers retain the exact old interface.
bool ReadIndices(IDirect3DDevice9* device,UINT start_index,
                 UINT triangle_count,INT base_vertex,UINT min_vertex,
                 UINT num_vertices,m2vr::drawprobe::VBVertexWindow& out) {
    return ReadIndicesTopology(device,D3DPT_TRIANGLELIST,
        start_index,triangle_count,base_vertex,min_vertex,num_vertices,out);
}
BOOL SubmitBufferWindow(IDirect3DDevice9* device,
                        const m2vr::drawprobe::VBVertexWindow& window,
                        UINT alpha_cutoff) {
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) ||
       window.indices.empty() || alpha_cutoff>256u ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread())
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

// Use original D3D9 stream-0 + optional source index buffer rather than
// mislabeling every original projected vertex as CPU-authenticated world.
// Only the separately PROVEN game's render-thread world transaction may
// request the extraction. The established UP projector adapter still owns
// same-thread/Present CPU join, source caller, projection fit, texture,
// sampler/raster policy and independent-eye submission.
BOOL SubmitProjectedBufferWindow(
    IDirect3DDevice9* device,
    const m2vr::drawprobe::VBVertexWindow& window,
    UINT expected_cpu_caller,UINT alpha_cutoff) {
    constexpr std::size_t max_indices=
        m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles*3u;
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) ||
       !expected_cpu_caller || alpha_cutoff>256u ||
       window.indices.size()<6u || window.indices.size()>max_indices ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread())
        return FALSE;
    try {
        std::vector<std::uint8_t> vertices;
        UINT stride=0;
        if(!CopyVBWindow(device,window,vertices,stride,true))
            return FALSE;
        DWORD fvf=0;
        if(FAILED(device->GetFVF(&fvf)))return FALSE;
        std::vector<std::uint8_t> ordered;
        if(!m2vr::drawprobe::ExpandProjectedVBWindow(
             vertices.data(),vertices.size(),fvf,stride,window,ordered,
             m2vr::drawprobe::ProjectedExpansionUse::AuthenticatedWorld))
            return FALSE;
        return Model2VR_AppendVerifiedProjectedD3D9UPTexturedWithSourceIds(
            device,ordered.data(),
            static_cast<UINT>(window.indices.size()/3u),
            stride,expected_cpu_caller,alpha_cutoff,
            window.indices.data(),static_cast<UINT>(window.indices.size()));
    }catch(...){return FALSE;}
}
} // namespace

// Mirrors DrawPrimitive's StartVertex and PrimitiveCount; TRIANGLELIST only.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9VBTextured(
    void* device,unsigned int start_vertex,
    unsigned int primitive_count,unsigned int alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::VertexBuffer);
    try {
        if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread())return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::VertexBuffer);
        m2vr::drawprobe::VBVertexWindow window{};
        if(!m2vr::drawprobe::RemapSequentialTriangleList(
                start_vertex,primitive_count,window))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::VertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitBufferWindow(static_cast<IDirect3DDevice9*>(device),
                                  window,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::VertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::VertexBuffer);}
}

// Mirrors DrawIndexedPrimitive's BaseVertexIndex/MinVertexIndex/
// NumVertices/StartIndex/PrimitiveCount; TRIANGLELIST only.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedVBTextured(
    void* device,int base_vertex_index,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int start_index,unsigned int primitive_count,
    unsigned int alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::IndexedVertexBuffer);
    try {
        if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread())return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::IndexedVertexBuffer);
        m2vr::drawprobe::VBVertexWindow window{};
        if(!ReadIndices(static_cast<IDirect3DDevice9*>(device),
            start_index,primitive_count,base_vertex_index,min_vertex_index,
            num_vertices,window))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::IndexedVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitBufferWindow(static_cast<IDirect3DDevice9*>(device),
                                  window,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::IndexedVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::IndexedVertexBuffer);}
}

// Explicit projected fixed-function VB companions. These are NEVER invoked
// by the generic D3D9 hook; only an independently authenticated ElSemi
// gameplay-world adapter may call them with a proven CPU projector RVA.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9VBTextured(
    void* device,unsigned int start_vertex,unsigned int primitive_count,
    unsigned int expected_cpu_caller,unsigned int alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedVertexBuffer);
    try {
        if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread())
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedVertexBuffer);
        m2vr::drawprobe::VBVertexWindow window{};
        if(!m2vr::drawprobe::RemapSequentialTriangleList(
               start_vertex,primitive_count,window))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitProjectedBufferWindow(
            static_cast<IDirect3DDevice9*>(device),window,
            expected_cpu_caller,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedVertexBuffer);}
}

extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9IndexedVBTextured(
    void* device,int base_vertex_index,
    unsigned int min_vertex_index,unsigned int num_vertices,
    unsigned int start_index,unsigned int primitive_count,
    unsigned int expected_cpu_caller,unsigned int alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedIndexedVertexBuffer);
    try {
        if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
           primitive_count<2u ||
           primitive_count>m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles)
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedIndexedVertexBuffer);
        m2vr::drawprobe::VBVertexWindow window{};
        if(!ReadIndices(static_cast<IDirect3DDevice9*>(device),
            start_index,primitive_count,base_vertex_index,min_vertex_index,
            num_vertices,window))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedIndexedVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitProjectedBufferWindow(
            static_cast<IDirect3DDevice9*>(device),window,
            expected_cpu_caller,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedIndexedVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedIndexedVertexBuffer);}
}

// New original VB/IB topology variants. No new emulator Draw call or
// source world-pass inference: the existing material/camera proof, Lock
// safety, and binocular renderer remain the only admission/commit path.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9VBTopologyTextured(
    void* device,unsigned type,unsigned start,unsigned count,unsigned cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyVertexBuffer);
    if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread())return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyVertexBuffer);
    try {
        m2vr::drawprobe::VBVertexWindow window{};
        if(!m2vr::drawprobe::RemapSequentialTopology(
             start,static_cast<D3DPRIMITIVETYPE>(type),count,window))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitBufferWindow(static_cast<IDirect3DDevice9*>(device),
                                  window,cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyVertexBuffer);}
}
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedVBTopologyTextured(
    void* device,unsigned type,int base,unsigned min,unsigned num,
    unsigned start,unsigned count,unsigned cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedVertexBuffer);
    if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread())return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedVertexBuffer);
    try {
        m2vr::drawprobe::VBVertexWindow window{};
        if(!ReadIndicesTopology(static_cast<IDirect3DDevice9*>(device),
            static_cast<D3DPRIMITIVETYPE>(type),start,count,base,min,num,window))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitBufferWindow(static_cast<IDirect3DDevice9*>(device),
                                  window,cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedVertexBuffer);}
}
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9VBTopologyTextured(
    void* device,unsigned type,unsigned start,unsigned count,
    unsigned caller,unsigned cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyVertexBuffer);
    if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       !caller || count<2u ||
       count>m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles)
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyVertexBuffer);
    try {
        m2vr::drawprobe::VBVertexWindow window{};
        if(!m2vr::drawprobe::RemapSequentialTopology(
            start,static_cast<D3DPRIMITIVETYPE>(type),count,window))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitProjectedBufferWindow(
            static_cast<IDirect3DDevice9*>(device),window,caller,cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyVertexBuffer);}
}
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9IndexedVBTopologyTextured(
    void* device,unsigned type,int base,unsigned min,unsigned num,
    unsigned start,unsigned count,unsigned caller,unsigned cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedVertexBuffer);
    if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       !caller || count<2u ||
       count>m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles)
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedVertexBuffer);
    try {
        m2vr::drawprobe::VBVertexWindow window{};
        if(!ReadIndicesTopology(static_cast<IDirect3DDevice9*>(device),
            static_cast<D3DPRIMITIVETYPE>(type),start,count,base,min,num,window))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedVertexBuffer);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (SubmitProjectedBufferWindow(
            static_cast<IDirect3DDevice9*>(device),window,caller,cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedVertexBuffer);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedVertexBuffer);}
}

// Observe a sampled actual D3D9 projected fixed-function VB/IB Draw. This
// function never enables world proof, touches emulator state or renders an
// extra Draw. A failed LOCK is an ordinary unsupported source, not an error.
namespace m2vr::drawprobe {
bool InspectProjectedD3D9VBTopologyJoin(
    IDirect3DDevice9* device,bool indexed,D3DPRIMITIVETYPE topology,
    UINT start_vertex,INT base_vertex,
    UINT min_vertex,UINT num_vertices,
    UINT start_index,UINT primitive_count,
    ProjectedTriangleBatchJoin& output) noexcept {
    if(!device || primitive_count<2u ||
       primitive_count>ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles)return false;
    try {
        VBVertexWindow window{};
        if(indexed){
            if(!ReadIndicesTopology(device,topology,
                            start_index,primitive_count,
                            base_vertex,min_vertex,num_vertices,window))
                return false;
        }else if(!RemapSequentialTopology(
                    start_vertex,topology,primitive_count,window))return false;
        std::vector<std::uint8_t> vertices,ordered;
        UINT stride=0;
        if(!CopyVBWindow(device,window,vertices,stride,true))
            return false;
        DWORD fvf=0;
        if(FAILED(device->GetFVF(&fvf)) ||
           !ExpandProjectedVBWindow(vertices.data(),vertices.size(),
                                    fvf,stride,window,ordered))
            return false;
        std::array<ProjectedDrawVertex,
                   ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles*3u> screen{};
        const std::size_t count=window.indices.size();
        for(std::size_t i=0;i<count;++i){
            ProjectedUPWorldVertex decoded{};
            if(!DecodeProjectedUPWorldVertex(ordered.data()+i*stride,
                                             fvf,stride,decoded))
                return false;
            screen[i]={decoded.screen_xy[0],decoded.screen_xy[1]};
        }
        output=ThreadProjectDrawCorrelator().MatchTriangleList(
            CurrentProjectDrawEpoch(),screen.data(),count,
            window.indices.data()); // true indexed/strip/fan vertices
        return true;
    }catch(...){return false;}
}
bool InspectProjectedD3D9VBJoin(
    IDirect3DDevice9* device,bool indexed,
    UINT start_vertex,INT base_vertex,
    UINT min_vertex,UINT num_vertices,
    UINT start_index,UINT primitive_count,
    ProjectedTriangleBatchJoin& output) noexcept {
    return InspectProjectedD3D9VBTopologyJoin(
        device,indexed,D3DPT_TRIANGLELIST,start_vertex,base_vertex,
        min_vertex,num_vertices,start_index,primitive_count,output);
}
} // namespace m2vr::drawprobe
