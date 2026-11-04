#pragma once
// Every explicit D3D9 world-source adapter propagates its failure to the
// same-thread source-frame gate; false admission must never silently leave
// previously accepted triangles eligible for OpenXR publication.
#include "verified_world_bridge.h"
#include "verified_world_source_epoch.hpp"
namespace m2vr::vrbridge {
inline BOOL CompleteOriginalD3D9SourceDraw(
    BOOL result,OriginalSourceDrawKind source_kind) noexcept {
    if(result==FALSE) {
        const auto outer=CurrentOriginalSourceDrawKind();
        Model2VR_RejectVerifiedD3D9SourceDraw(
            outer?outer:static_cast<unsigned int>(source_kind));
    }
    return result;
}
} // namespace m2vr::vrbridge
