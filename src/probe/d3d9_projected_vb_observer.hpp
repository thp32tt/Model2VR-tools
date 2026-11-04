#pragma once
// Read-only original D3D9 VB/IB -> same-thread CPU projector correspondence.
// A successful correlation is a CANDIDATE, never gameplay world permission.
#include "project_draw_correlator.hpp"
#include <d3d9.h>
namespace m2vr::drawprobe {
bool InspectProjectedD3D9VBJoin(
    IDirect3DDevice9* device,bool indexed,
    UINT start_vertex,INT base_vertex,
    UINT min_vertex,UINT num_vertices,
    UINT start_index,UINT primitive_count,
    ProjectedTriangleBatchJoin& result) noexcept;
} // namespace m2vr::drawprobe
