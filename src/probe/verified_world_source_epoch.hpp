#pragma once
// Once an authenticated world source Draw fails, a partially rasterized pair
// must not be republished or rebuilt for that physical original Present.
// This is frame accounting only; it never authenticates a Daytona world pass.
#include <cstdint>

namespace m2vr::vrbridge {
// Stable diagnostic IDs, never gameplay-world/HUD proof.
enum class OriginalSourceDrawKind : std::uint32_t {
    NativeCpuRaster=0u,
    PrimitiveUP=1u,
    ProjectedPrimitiveUP=2u,
    IndexedUP=3u,
    ProjectedIndexedUP=4u,
    TopologyPrimitiveUP=5u,
    ProjectedTopologyPrimitiveUP=6u,
    VertexBuffer=7u,
    IndexedVertexBuffer=8u,
    ProjectedVertexBuffer=9u,
    ProjectedIndexedVertexBuffer=10u,
    TopologyVertexBuffer=11u,
    TopologyIndexedVertexBuffer=12u,
    ProjectedTopologyVertexBuffer=13u,
    ProjectedTopologyIndexedVertexBuffer=14u,
    TopologyIndexedUP=15u,
    ProjectedTopologyIndexedUP=16u,
};
// Source provenance belongs to the ORIGINAL outermost D3D9 adapter, not
// to a nested UP/vertex-buffer helper. This local-only tag carries neither
// world permission nor image bytes across process/IPC boundaries.
inline thread_local OriginalSourceDrawKind g_current_original_source_kind=
    OriginalSourceDrawKind::NativeCpuRaster;
inline std::uint32_t CurrentOriginalSourceDrawKind() noexcept {
    return static_cast<std::uint32_t>(g_current_original_source_kind);
}
enum class OriginalSourceFailure : std::uint32_t {
    Unknown=0u, Device=1u, Shader=2u, VertexPipeline=3u,
    Raster=4u, BlendAndLighting=5u, StageZero=6u,
    StageOneAndAlphaTest=7u, Sampler=8u, TargetAndDepth=9u, ProjectionPass=10u,
    // Physical original Present/Reset lifecycle rejection, not game-world
    // authentication. Stable IDs for same-frame source failure diagnosis.
    PresentUnarmed=11u, PresentStale=12u,
    PresentRejected=13u, DeviceReset=14u, PresentFrameMismatch=15u,
    OriginalSceneFailed=16u, OriginalDrawFailed=17u,
    OriginalClearFailed=18u, OriginalSurfaceFailed=19u,
    OriginalLineageMismatch=20u, OriginalStateBlockUnknown=21u,
    OriginalInstancedStreamUnknown=22u,
    OriginalTextureUploadAfterStereo=23u,
    OriginalPaletteMutationAfterStereo=24u,
    OriginalRasterModeUnknown=25u,
    OriginalUnmirroredGeometry=26u,
    OriginalBufferContentAfterStereo=27u,
    OriginalBufferHookCoverageUnknown=28u,
    OriginalGpuSurfaceMutationAfterStereo=29u
};
inline thread_local OriginalSourceFailure g_current_original_source_failure=
    OriginalSourceFailure::Unknown;
inline std::uint32_t CurrentOriginalSourceFailure() noexcept {
    return static_cast<std::uint32_t>(g_current_original_source_failure);
}
inline bool RejectOriginalSourceMaterial(OriginalSourceFailure reason) noexcept {
    if(g_current_original_source_failure==OriginalSourceFailure::Unknown)
        g_current_original_source_failure=reason;
    return false;
}
class ScopedOriginalSourceDrawKind final {
public:
    explicit ScopedOriginalSourceDrawKind(OriginalSourceDrawKind next) noexcept
        : previous_(g_current_original_source_kind),
          previous_failure_(g_current_original_source_failure) {
        if(previous_==OriginalSourceDrawKind::NativeCpuRaster) {
            g_current_original_source_kind=next;
            g_current_original_source_failure=OriginalSourceFailure::Unknown;
        }
    }
    ~ScopedOriginalSourceDrawKind() noexcept {
        g_current_original_source_kind=previous_;
        g_current_original_source_failure=previous_failure_;
    }
    ScopedOriginalSourceDrawKind(const ScopedOriginalSourceDrawKind&)=delete;
    ScopedOriginalSourceDrawKind& operator=(const ScopedOriginalSourceDrawKind&)=delete;
private:
    OriginalSourceDrawKind previous_;
    OriginalSourceFailure previous_failure_;
};
class OriginalSourceFrameEpoch {
public:
    constexpr bool CanBegin(std::uint64_t frame,
                            std::uint64_t completed) const noexcept {
        return frame!=0u && frame>completed && frame>rejected_;
    }
    // True only for the first rejection of this frame/epoch. Monotonic
    // original Present IDs prevent rewinding into an incomplete world.
    constexpr bool Revoke(std::uint64_t frame) noexcept {
        if(frame==0u || frame<=rejected_)return false;
        rejected_=frame;
        return true;
    }
    constexpr std::uint64_t LastRejected() const noexcept {
        return rejected_;
    }
private:
    std::uint64_t rejected_=0u;
};
} // namespace m2vr::vrbridge
