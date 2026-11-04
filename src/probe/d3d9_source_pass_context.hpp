#pragma once
// Compact read-only D3D9 render-source context for original Draw witnesses.
// No classification bit can grant world/HUD ownership. Unknown query results
// remain explicitly unknown, not treated as valid main-scene geometry.
#include <cstdint>

namespace m2vr::drawprobe {
enum class OriginalTargetClass : std::uint32_t {
    Unknown=0u, MainBackbuffer=1u, Auxiliary=2u
};
inline constexpr std::uint32_t kSourceTargetMask=3u;
inline constexpr std::uint32_t kSourceViewportKnown=1u<<2u;
inline constexpr std::uint32_t kSourceViewportFull=1u<<3u;
inline constexpr std::uint32_t kSourceDepthKnown=1u<<4u;
inline constexpr std::uint32_t kSourceDepthEnabled=1u<<5u;
inline constexpr std::uint32_t kSourceDepthWrite=1u<<6u;
inline constexpr std::uint32_t kSourceScissorKnown=1u<<7u;
inline constexpr std::uint32_t kSourceScissorEnabled=1u<<8u;
inline constexpr std::uint32_t kSourceTargetDescKnown=1u<<9u;
inline constexpr std::uint32_t kSourceDepthSurfaceKnown=1u<<10u;
inline constexpr std::uint32_t kSourceDepthSurfaceCompatible=1u<<11u;
inline constexpr std::uint32_t kSourceScissorRectKnown=1u<<12u;
inline constexpr std::uint32_t kSourceScissorFull=1u<<13u;
struct OriginalDrawSourceContext {
    OriginalTargetClass target=OriginalTargetClass::Unknown;
    bool viewport_known=false;
    bool viewport_full=false;
    bool depth_known=false;
    bool depth_enabled=false;
    bool depth_write=false;
    bool scissor_known=false;
    bool scissor_enabled=false;
    bool scissor_rect_known=false;
    bool scissor_full=false;
    bool target_desc_known=false;
    bool depth_surface_known=false;
    bool depth_surface_compatible=false;
    std::uint32_t source_width=0u,source_height=0u;
};
inline constexpr std::uint32_t EncodeOriginalDrawSourceContext(
    const OriginalDrawSourceContext& c) noexcept {
    // A view is "full" only for a confirmed backbuffer with known source
    // dimensions; never promote a plausible-looking auxiliary viewport.
    return static_cast<std::uint32_t>(c.target) |
        (c.viewport_known?kSourceViewportKnown:0u) |
        (c.viewport_known && c.viewport_full && c.target_desc_known &&
             c.source_width>0u && c.source_height>0u &&
             c.target==OriginalTargetClass::MainBackbuffer ?
             kSourceViewportFull:0u) |
        (c.depth_known?kSourceDepthKnown:0u) |
        (c.depth_known && c.depth_enabled?kSourceDepthEnabled:0u) |
        (c.depth_known && c.depth_write?kSourceDepthWrite:0u) |
        (c.scissor_known?kSourceScissorKnown:0u) |
        (c.scissor_known && c.scissor_enabled?kSourceScissorEnabled:0u) |
        (c.scissor_known && c.scissor_enabled && c.scissor_rect_known ?
            kSourceScissorRectKnown:0u) |
        (c.scissor_known && c.scissor_enabled && c.scissor_rect_known &&
         c.scissor_full && c.target_desc_known &&
         c.source_width>0u && c.source_height>0u &&
         c.target==OriginalTargetClass::MainBackbuffer ?
            kSourceScissorFull:0u) |
        (c.target_desc_known?kSourceTargetDescKnown:0u) |
        (c.depth_surface_known?kSourceDepthSurfaceKnown:0u) |
        (c.depth_surface_known && c.depth_surface_compatible ?
             kSourceDepthSurfaceCompatible:0u);
}
} // namespace m2vr::drawprobe
