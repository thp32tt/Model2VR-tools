// Explicit original DrawPrimitiveUP triangle strip/fan/list -> proof-owned
// D3D9 material + CPU projector + two-eye raster path. Original game Draw,
// simulation, FFB, Present and 2D fallback are never called or altered here.
#include "verified_world_bridge.h"
#include "verified_world_source_result.hpp"
#include "d3d9_primitive_topology.hpp"
#include "d3d9_vb_index_window.hpp"
#include "project_draw_correlator.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
namespace {
bool SafeCopyOriginal(void* dest,const void* src,std::size_t bytes) noexcept {
    if(!dest || !src || !bytes || bytes>512u*1024u)return false;
    __try{std::memcpy(dest,src,bytes);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool SourceTriangleIndices(D3DPRIMITIVETYPE type,unsigned triangles,
     std::vector<std::uint32_t>& result,unsigned& count) {
    count=m2vr::drawprobe::PrimitiveSourceElementCount(type,triangles);
    if(!count || count>2048u)return false;
    std::vector<std::uint32_t> source(count);
    for(unsigned i=0;i<count;++i)source[i]=i;
    return m2vr::drawprobe::ExpandOriginalTriangles(
        type,triangles,source,result);
}
} // namespace

// For original XYZ/TEX1 fixed-function UP; uses real D3D9 W/V/P and source
// texture/raster state in the established verified renderer.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedD3D9PrimitiveUPTopologyTextured(
    void* device,unsigned type,unsigned triangles,
    const void* source_vertices,unsigned stride,unsigned alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyPrimitiveUP);
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) || !source_vertices || !stride || stride>256u ||
       alpha_cutoff>256u ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread())
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyPrimitiveUP);
    try {
        std::vector<std::uint32_t> indices;
        unsigned count=0u;
        if(!SourceTriangleIndices(static_cast<D3DPRIMITIVETYPE>(type),
             triangles,indices,count))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyPrimitiveUP);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedD3D9UPTextured(
            device,source_vertices,count,stride,indices.data(),
            static_cast<unsigned>(indices.size()),alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyPrimitiveUP);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::TopologyPrimitiveUP);}
}

// For original XYZRHW/TEX1 UP. Only 2..8 complete original triangles
// matched to the independent authenticated CPU projected source caller
// can enter the calibrated HMD stereo branch.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedProjectedD3D9PrimitiveUPTopologyTextured(
    void* device,unsigned type,unsigned triangles,
    const void* source_vertices,unsigned stride,
    unsigned authenticated_cpu_project_caller_rva,unsigned alpha_cutoff) {
    const m2vr::vrbridge::ScopedOriginalSourceDrawKind source_scope(
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
    if(!device || !Model2VR_IsOriginalPrimaryD3D9Device(device) || !source_vertices || !authenticated_cpu_project_caller_rva ||
       !stride || stride>256u || alpha_cutoff>256u ||
       triangles<2u ||
       triangles>m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles ||
       !Model2VR_VerifiedWorldFrameOwnedByCurrentThread())
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
    try {
        std::vector<std::uint32_t> indices;
        unsigned count=0u;
        if(!SourceTriangleIndices(static_cast<D3DPRIMITIVETYPE>(type),
             triangles,indices,count))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
        auto* d=static_cast<IDirect3DDevice9*>(device);
        DWORD fvf=0u;
        if(FAILED(d->GetFVF(&fvf)))return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
        std::vector<std::uint8_t> original(std::size_t(count)*stride);
        if(!SafeCopyOriginal(original.data(),source_vertices,original.size()))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
        m2vr::drawprobe::VBVertexWindow window{};
        window.first=0u;window.count=count;
        window.indices=std::move(indices);
        std::vector<std::uint8_t> ordered;
        if(!m2vr::drawprobe::ExpandProjectedVBWindow(
            original.data(),original.size(),fvf,stride,window,ordered))
            return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
        return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (Model2VR_AppendVerifiedProjectedD3D9UPTextured(
            device,ordered.data(),triangles,stride,
            authenticated_cpu_project_caller_rva,alpha_cutoff)),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);
    }catch(...){return m2vr::vrbridge::CompleteOriginalD3D9SourceDraw(
        (FALSE),
        m2vr::vrbridge::OriginalSourceDrawKind::ProjectedTopologyPrimitiveUP);}
}
