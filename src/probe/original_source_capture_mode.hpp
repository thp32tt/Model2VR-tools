#pragma once
// Pure original-game x86 hook/capture activation contract. Native OpenXR
// stereo is an explicit opt-in separate from passive read-only diagnostics.
// No source camera, world/HUD permission or emulator address is inferred.
namespace m2vr::drawprobe {
inline constexpr bool NeedOriginalD3D9Hooks(
    bool passive_d3d9_probe,bool native_stereo) noexcept {
    return passive_d3d9_probe || native_stereo;
}
inline constexpr bool CaptureOriginalCpuProjector(
    bool passive_d3d9_probe,bool native_stereo,
    bool signed_projector_hook_enabled) noexcept {
    return NeedOriginalD3D9Hooks(passive_d3d9_probe,native_stereo) &&
           signed_projector_hook_enabled;
}
inline constexpr bool OriginalCpuProjectorProofInputsReady(
    bool passive_d3d9_probe,bool native_stereo,bool projector_hook,
    bool active_matrix_source,bool geometry_stream_source) noexcept {
    return CaptureOriginalCpuProjector(
               passive_d3d9_probe,native_stereo,projector_hook) &&
           active_matrix_source && geometry_stream_source;
}
} // namespace m2vr::drawprobe
