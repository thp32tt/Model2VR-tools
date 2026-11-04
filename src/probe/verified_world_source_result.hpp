#pragma once
// Every explicit D3D9 world-source adapter propagates its failure to the
// same-thread source-frame gate; false admission must never silently leave
// previously accepted triangles eligible for OpenXR publication.
#include "verified_world_bridge.h"
namespace m2vr::vrbridge {
inline BOOL CompleteOriginalD3D9SourceDraw(
    BOOL result,unsigned int source_kind) noexcept {
    if(result==FALSE)Model2VR_RejectVerifiedD3D9SourceDraw(source_kind);
    return result;
}
} // namespace m2vr::vrbridge
