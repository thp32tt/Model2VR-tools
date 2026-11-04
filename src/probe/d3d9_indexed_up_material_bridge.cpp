// Proof-owned original indexed-UP source -> existing calibrated two-eye
// textured renderer. This neither hooks nor invokes a second original Draw.
#include "verified_world_bridge.h"
#include "verified_world_source_result.hpp"
#include "d3d9_indexed_up_window.hpp"
#include "d3d9_vb_index_window.hpp"
#include "d3d9_primitive_topology.hpp"
#include "project_draw_correlator.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
namespace {
bool ProtectedCopy(void* dst,const void* src,std::size_t size) noexcept {
    if(!dst || !src || !size || size>512u*1024u)return false;
    __try {std::memcpy(dst,src,size);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool CaptureIndexedUP(
    IDirect3DDevice9* device,D3DPRIMITIVETYPE primitive_type,
    unsigned min_vertex,unsigned vertex_count,unsigned triangle_count,
    const void* indices,D3DFORMAT format,
    const void* vertices,unsigned stride,
    std::vector<std::uint8_t>& source,
    std::vector<std::uint32_t>& relative,unsigned& copied_count) {
    using namespace m2vr::drawprobe;
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) ||
       !indices || !vertices || !stride || stride>256u ||
       !vertex_count || vertex_count>kMaxIndexedUPDeclaredVertices ||
       !triangle_count || triangle_count>kMaxIndexedUPTriangles ||
       (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32))
        return false;
    const unsigned element_count=PrimitiveSourceElementCount(
        primitive_type,triangle_count);
    if(!element_count)return false;
    const std::size_t width=format==D3DFMT_INDEX16?2u:4u;
    const std::size_t index_bytes=std::size_t(element_count)*width;
    std::vector<std::uint8_t> original_indices(index_bytes);
    if(!ProtectedCopy(original_indices.data(),indices,index_bytes))
        return false;
    std::vector<std::uint32_t> original_order;
    if(!RemapIndexedUPSourceElements(original_indices.data(),index_bytes,
        format,min_vertex,vertex_count,element_count,original_order))
        return false;
    std::vector<std::uint32_t> triangles;
    if(!ExpandOriginalTriangles(
        primitive_type,triangle_count,original_order,triangles))
        return false;
    IndexedUPCopiedWindow window{};
    if(!CompactIndexedUPIndexWindow(triangles,vertex_count,window))
        return false;
    const std::size_t offset=std::size_t(window.source_offset)*stride;
    const std::size_t vertex_bytes=std::size_t(window.count)*stride;
    if(vertex_bytes>512u*1024u)return false;
    const auto address=reinterpret_cast<std::uintptr_t>(vertices);
    if(std::uint64_t(address)+offset>UINTPTR_MAX)return false;
    std::vector<std::uint8_t> original_vertices(vertex_bytes);
    if(!ProtectedCopy(original_vertices.data(),
        reinterpret_cast<const void*>(address+offset),vertex_bytes))
        return false;
    // Even if the D3D9 call declares 65K vertices, a two-triangle
    // sample now copies ONLY its referenced span. All indices are local.
    relative=std::move(window.indices);
    copied_count=window.count;
    source=std::move(original_vertices);
    return true;
}
} // namespace

extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedUPTopologyTextured(
    void* device,unsigned type,unsigned min_vertex,unsigned vertex_count,
    unsigned triangles,const void* indices,unsigned format,
    const void* vertices,unsigned stride,unsigned alpha_cutoff) {
    if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       alpha_cutoff>256u)return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedUP);
    try {
        std::vector<std::uint8_t> source;
        std::vector<std::uint32_t> relative;
        unsigned copied_count=0u;
        if(!CaptureIndexedUP(static_cast<IDirect3DDevice9*>(device),
            static_cast<D3DPRIMITIVETYPE>(type),min_vertex,vertex_count,triangles,indices,
            static_cast<D3DFORMAT>(format),vertices,stride,
            source,relative,copied_count))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedUP);
        // Existing UP route verifies XYZ/TEX1, world/view/projection,
        // original texture/sampler/Z state and owned stereo frame.
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedD3D9UPTextured(
            device,source.data(),copied_count,stride,relative.data(),
            static_cast<unsigned>(relative.size()),alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedUP);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyIndexedUP);}
}

extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9IndexedUPTopologyTextured(
    void* device,unsigned type,unsigned min_vertex,unsigned vertex_count,
    unsigned triangles,const void* indices,unsigned format,
    const void* vertices,unsigned stride,unsigned cpu_caller,
    unsigned alpha_cutoff) {
    if(!Model2VR_VerifiedWorldFrameOwnedByCurrentThread() ||
       !cpu_caller || alpha_cutoff>256u || triangles<2u ||
       triangles>m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles)
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedUP);
    try {
        std::vector<std::uint8_t> source,expanded;
        std::vector<std::uint32_t> relative;
        unsigned copied_count=0u;
        auto* d=static_cast<IDirect3DDevice9*>(device);
        if(!CaptureIndexedUP(d,static_cast<D3DPRIMITIVETYPE>(type),
            min_vertex,vertex_count,triangles,
            indices,static_cast<D3DFORMAT>(format),vertices,stride,
            source,relative,copied_count))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedUP);
        DWORD fvf=0;
        if(FAILED(d->GetFVF(&fvf)))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedUP);
        m2vr::drawprobe::VBVertexWindow window{};
        window.first=0u;window.count=copied_count;
        window.indices=std::move(relative);
        if(!m2vr::drawprobe::ExpandProjectedVBWindow(
            source.data(),source.size(),fvf,stride,window,expanded))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedUP);
        // Exact original triangle order then same-thread/Present CPU
        // projector join, perspective fit and texture/Z/pose checks.
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedProjectedD3D9UPTextured(
            device,expanded.data(),triangles,stride,cpu_caller,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedUP);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyIndexedUP);}
}

// Preserve the previously shipped public TRIANGLELIST ABI for existing
// separately authenticated world adapters. No new automatic hook.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9IndexedUPTextured(
    void* device,unsigned min_vertex,unsigned vertex_count,
    unsigned triangles,const void* indices,unsigned format,
    const void* vertices,unsigned stride,unsigned alpha_cutoff) {
    return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedD3D9IndexedUPTopologyTextured(
        device,D3DPT_TRIANGLELIST,min_vertex,vertex_count,triangles,
        indices,format,vertices,stride,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::IndexedUP);
}
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9IndexedUPTextured(
    void* device,unsigned min_vertex,unsigned vertex_count,
    unsigned triangles,const void* indices,unsigned format,
    const void* vertices,unsigned stride,unsigned cpu_caller,
    unsigned alpha_cutoff) {
    return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedProjectedD3D9IndexedUPTopologyTextured(
        device,D3DPT_TRIANGLELIST,min_vertex,vertex_count,triangles,
        indices,format,vertices,stride,cpu_caller,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedIndexedUP);
}
