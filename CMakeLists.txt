cmake_minimum_required(VERSION 3.24)

project(Model2VRTools LANGUAGES C CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Model2VRProbe currently targets the 32-bit Windows Model 2 Emulator.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Configure with a 32-bit generator (for MSVC: -A Win32).")
endif()

include(FetchContent)

FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG v1.3.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(minhook)

add_library(model2vr_probe SHARED
  src/probe/config.cpp
  src/probe/dllmain.cpp
  src/probe/hooks_x86.cpp
  src/probe/logger.cpp
  src/probe/signature.cpp
  src/probe/stereo_feed.cpp
  src/probe/verified_world_bridge.cpp
  src/probe/d3d9_up_material_bridge.cpp
  src/probe/d3d9_vb_material_bridge.cpp
  src/probe/d3d9_indexed_up_material_bridge.cpp
  src/probe/d3d9_up_topology_material_bridge.cpp
  src/probe/d3d9_probe.cpp
  src/probe/daytona_native_stereo.cpp
)

target_include_directories(model2vr_probe PRIVATE src/probe)
target_link_libraries(model2vr_probe PRIVATE minhook)
target_compile_features(model2vr_probe PRIVATE cxx_std_20)
target_compile_definitions(model2vr_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(model2vr_probe PRIVATE /W4 /permissive- /EHsc)
endif()

set_target_properties(model2vr_probe PROPERTIES
  OUTPUT_NAME "Model2VRProbe"
)

add_executable(model2vr_loader
  src/loader/main.cpp
)

# Win32 F11 FFB/input control panel. FFB output remains in the separately
# distributed Generic DD plugin; never link a second haptic backend here.
add_library(model2vr_ffb_ini STATIC src/ffb/ini_edit_transaction.cpp)
target_compile_features(model2vr_ffb_ini PUBLIC cxx_std_20)
target_compile_definitions(model2vr_ffb_ini PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
  target_compile_options(model2vr_ffb_ini PRIVATE /W4 /permissive- /EHsc)
endif()
add_executable(model2vr_ffb_control_panel WIN32
  src/ffb/control_panel.cpp
  src/ffb/binding_editor.cpp
)
target_link_libraries(model2vr_ffb_control_panel PRIVATE model2vr_ffb_ini)
target_compile_features(model2vr_ffb_control_panel PRIVATE cxx_std_20)
target_compile_definitions(model2vr_ffb_control_panel PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
  target_compile_options(model2vr_ffb_control_panel PRIVATE /W4 /permissive- /EHsc)
endif()
set_target_properties(model2vr_ffb_control_panel PROPERTIES
  OUTPUT_NAME "Model2VRControlPanel")
install(TARGETS model2vr_ffb_control_panel RUNTIME DESTINATION .)


target_compile_features(model2vr_loader PRIVATE cxx_std_20)
target_compile_definitions(model2vr_loader PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
  target_compile_options(model2vr_loader PRIVATE /W4 /permissive- /EHsc)
endif()

set_target_properties(model2vr_loader PROPERTIES
  OUTPUT_NAME "Model2VRLoader"
)

install(TARGETS model2vr_probe model2vr_loader
  RUNTIME DESTINATION .
  LIBRARY DESTINATION .
)
install(FILES config/model2vr_probe.example.ini DESTINATION .)
# Opt-in Daytona USA desktop 2D source-probe bundle. The launcher copies
# the read-only config to model2vr_probe.ini when the user runs this test.
# Never ship the emulator, ROM, original FFB plugin or user traces.
install(FILES
  config/daytona_2d_probe.ini
  scripts/Run-Daytona2D-Probe.cmd
  docs/DAYTONA_2D_PROBE_GUIDE.md
  DESTINATION .
)


# Portable stereo math, isolated from the Model2 binary and VR runtime.
add_library(model2vr_stereo_core STATIC
  src/vr/stereo_core.cpp
)
target_include_directories(model2vr_stereo_core PUBLIC src/vr)
target_compile_features(model2vr_stereo_core PUBLIC cxx_std_20)

enable_testing()
add_executable(model2vr_stereo_core_test tests/stereo_core_test.cpp)
target_link_libraries(model2vr_stereo_core_test PRIVATE model2vr_stereo_core)
add_test(NAME stereo_geometry_invariants COMMAND model2vr_stereo_core_test)

# 32/64-bit ABI-compatible shared stereo transport and diagnostic producer.
add_executable(model2vr_stereo_ipc_test tests/stereo_frame_ipc_test.cpp)
target_compile_features(model2vr_stereo_ipc_test PRIVATE cxx_std_20)
add_test(NAME stereo_frame_transport COMMAND model2vr_stereo_ipc_test)
# Verify bounded 60Hz producer -> 90/120Hz HMD holding and expiry.
add_executable(model2vr_frame_policy_test tests/stereo_frame_policy_test.cpp)
target_compile_features(model2vr_frame_policy_test PRIVATE cxx_std_20)
add_test(NAME stereo_frame_rate_gate COMMAND model2vr_frame_policy_test)

add_executable(model2vr_stereo_diagnostic tools/model2_stereo_diagnostic.cpp)
target_compile_features(model2vr_stereo_diagnostic PRIVATE cxx_std_20)
set_target_properties(model2vr_stereo_diagnostic PROPERTIES
  OUTPUT_NAME "Model2VRStereoDiagnostic")
install(TARGETS model2vr_stereo_diagnostic RUNTIME DESTINATION .)


add_executable(model2vr_pose_ipc_test tests/stereo_pose_ipc_test.cpp)
target_compile_features(model2vr_pose_ipc_test PRIVATE cxx_std_20)
add_test(NAME stereo_predicted_pose_ipc COMMAND model2vr_pose_ipc_test)
add_executable(daytona_producer_health_x86_test tests/daytona_producer_health_test.cpp)
target_compile_features(daytona_producer_health_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_native_producer_health_ipc_x86
         COMMAND daytona_producer_health_x86_test)

add_executable(model2_render_policy_test tests/model2_render_policy_test.cpp)
target_compile_features(model2_render_policy_test PRIVATE cxx_std_20)
add_test(NAME model2_renderer_stereo_safety COMMAND model2_render_policy_test)

add_executable(model2_cpu_stereo_test tests/model2_cpu_stereo_test.cpp)
target_compile_features(model2_cpu_stereo_test PRIVATE cxx_std_20)
add_test(NAME model2_cpu_eye_projection COMMAND model2_cpu_stereo_test)

add_executable(model2_stereo_mesh_test tests/model2_stereo_mesh_test.cpp)
target_compile_features(model2_stereo_mesh_test PRIVATE cxx_std_20)
add_test(NAME model2_stereo_mesh_transaction COMMAND model2_stereo_mesh_test)

# Coherent one-pose-per-game-frame latch (no emulator execution).
add_executable(model2_frame_latch_test tests/model2_frame_latch_test.cpp)
target_compile_features(model2_frame_latch_test PRIVATE cxx_std_20)
add_test(NAME model2_frame_pose_latch COMMAND model2_frame_latch_test)

# Independently clipped eye draw streams, requiring a single frame pose latch.
add_executable(model2_eye_clip_test tests/model2_eye_clip_test.cpp)
target_compile_features(model2_eye_clip_test PRIVATE cxx_std_20)
add_test(NAME model2_eye_clip_independent_streams COMMAND model2_eye_clip_test)
# Actual opaque RGBA8 left/right targets with independent per-eye Z buffers,
# source-triangle color provenance and clipped world geometry, no FFB calls.
add_executable(model2_stereo_raster_test tests/model2_stereo_raster_test.cpp)
target_compile_features(model2_stereo_raster_test PRIVATE cxx_std_20)
add_test(NAME model2_native_opaque_stereo_raster COMMAND model2_stereo_raster_test)
# Per-eye homogeneous UV clipping, perspective-correct texture sampling,
# alpha-cutout and Z ordering. Synthetic CTest: no ROM/headset/FFB required.
add_executable(model2_textured_stereo_test tests/model2_textured_stereo_test.cpp)
target_compile_features(model2_textured_stereo_test PRIVATE cxx_std_20)
add_test(NAME model2_native_textured_stereo_raster COMMAND model2_textured_stereo_test)
# Source D3D9 POINT/LINEAR + CLAMP/WRAP sampling, bilinear alpha and
# fail-closed fixed-function state mapping (independent of HMD/runtime).
add_executable(model2_texture_sampler_test tests/model2_texture_sampler_test.cpp)
target_compile_features(model2_texture_sampler_test PRIVATE cxx_std_20)
target_compile_definitions(model2_texture_sampler_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_native_d3d9_texture_sampler COMMAND model2_texture_sampler_test)
# Per-vertex D3DCOLOR stage-zero TEXTURE x DIFFUSE contracts: reject
# unmodelled texture stages rather than silently discoloring stereo worlds.
add_executable(model2_d3d9_fixed_stage_test
    tests/model2_d3d9_fixed_stage_policy_test.cpp)
target_compile_features(model2_d3d9_fixed_stage_test PRIVATE cxx_std_20)
target_compile_definitions(model2_d3d9_fixed_stage_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_real_d3d9_diffuse_stage_admission
    COMMAND model2_d3d9_fixed_stage_test)
# Original D3D9 raster-state parity: per-eye CW/CCW culling, LESS/LEQUAL
# depth tie-break and ZWRITE control with source-state fail-closed admission.
add_executable(model2_d3d9_depth_cull_test tests/model2_d3d9_depth_cull_test.cpp)
target_compile_features(model2_d3d9_depth_cull_test PRIVATE cxx_std_20)
target_compile_definitions(model2_d3d9_depth_cull_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_native_d3d9_depth_cull_occlusion COMMAND model2_d3d9_depth_cull_test)


# Exact Daytona real D3D9 stream-zero 32-byte vertex declaration and
# finite source position/UV decoder: no emulator ROM/HMD needed.
add_executable(daytona_up_decl_witness_test tests/daytona_up_decl_witness_test.cpp)
target_compile_features(daytona_up_decl_witness_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_up_decl_witness_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_real_d3d9_decl_position_source_guard
    COMMAND daytona_up_decl_witness_test)
# Source-verified shader-UP source geometry: shift safe triangles alone.
add_executable(daytona_triangle_shift_x86_test tests/daytona_triangle_eye_shift_test.cpp)
target_compile_features(daytona_triangle_shift_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_verified_triangle_eye_shift_x86
         COMMAND daytona_triangle_shift_x86_test)

# Native surface transfer policy contract (x86).
add_executable(daytona_surface_transfer_x86_test tests/daytona_surface_transfer_gate_test.cpp)
target_compile_features(daytona_surface_transfer_x86_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_surface_transfer_x86_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_surface_transfer_x86 COMMAND daytona_surface_transfer_x86_test)

# The game-backbuffer HMD fallback never masquerades as stereoscopic world.
add_executable(daytona_original_mono_fallback_x86_test
    tests/daytona_original_mono_fallback_test.cpp)
target_compile_features(daytona_original_mono_fallback_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_present_mono_fallback_x86
    COMMAND daytona_original_mono_fallback_x86_test)

# Source-pixel visibility aggregates never publish raw original game pixels.
add_executable(daytona_mono_content_evidence_x86_test
    tests/daytona_mono_content_evidence_test.cpp)
target_compile_features(daytona_mono_content_evidence_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_backbuffer_visibility_evidence_x86
    COMMAND daytona_mono_content_evidence_x86_test)

# Source D3D9 BGRA eye readback to fixed RGBA OpenXR transport.
add_executable(daytona_readback_scale_x86_test tests/daytona_readback_scale_test.cpp)
target_compile_features(daytona_readback_scale_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_native_readback_scale_x86 COMMAND daytona_readback_scale_x86_test)

# Read-only, allocation-free UP vertex witness parser; no emulator runtime.
add_executable(model2_up_witness_test tests/model2_up_witness_test.cpp)
target_compile_features(model2_up_witness_test PRIVATE cxx_std_20)
add_test(NAME model2_bounded_up_vertex_witness COMMAND model2_up_witness_test)
# Strict D3D9 fixed-function XYZ+UV source decoding and native texture
# BGRA -> RGBA conversion; does not require hardware or actual game.
add_executable(model2_d3d9_world_source_test tests/model2_d3d9_world_source_test.cpp)
target_compile_features(model2_d3d9_world_source_test PRIVATE cxx_std_20)
add_test(NAME model2_real_d3d9_world_up_source_layout
    COMMAND model2_d3d9_world_source_test)
# CPU-readable original D3D9 DXT1/DXT3/DXT5 source materials must decode
# to the same RGBA8 texture stage used by the world eye renderer.
add_executable(model2_d3d9_dxt_source_test
    tests/model2_d3d9_dxt_decode_test.cpp)
target_compile_features(model2_d3d9_dxt_source_test PRIVATE cxx_std_20)
add_test(NAME model2_d3d9_verified_world_dxt_material_source
    COMMAND model2_d3d9_dxt_source_test)
# Authenticated x86 CPU-lockable L8/A8L8 original texture bytes also
# feed the existing x86 RGBA frames -> Win64 OpenXR eye images.
# Additional original fixed-function RGB8/RGB24 source material decoder.
add_executable(model2_d3d9_rgb_byte_source_test
    tests/model2_d3d9_rgb_byte_decode_test.cpp)
target_compile_features(model2_d3d9_rgb_byte_source_test PRIVATE cxx_std_20)
add_test(NAME model2_d3d9_verified_world_rgb_byte_material_source
    COMMAND model2_d3d9_rgb_byte_source_test)
# Genuine D3D9 A8 alpha-only source -> RGBA8 before eye alpha policy.
add_executable(model2_d3d9_palette_source_test
    tests/model2_d3d9_palette_decode_test.cpp)
target_compile_features(model2_d3d9_palette_source_test PRIVATE cxx_std_20)
add_test(NAME model2_d3d9_verified_world_palette_material_source
    COMMAND model2_d3d9_palette_source_test)

add_executable(model2_d3d9_alpha_only_source_test
    tests/model2_d3d9_alpha_only_decode_test.cpp)
target_compile_features(model2_d3d9_alpha_only_source_test PRIVATE cxx_std_20)
add_test(NAME model2_d3d9_verified_world_alpha_only_source
    COMMAND model2_d3d9_alpha_only_source_test)
add_executable(model2_d3d9_luminance_source_test
    tests/model2_d3d9_luminance_decode_test.cpp)
target_compile_features(model2_d3d9_luminance_source_test PRIVATE cxx_std_20)
add_test(NAME model2_d3d9_verified_world_luminance_alpha
    COMMAND model2_d3d9_luminance_source_test)
# Old arcade original CPU source textures also use losslessly mappable
# 16-bit BGRA packed pixel formats; no GPU or device state needed here.
add_executable(model2_d3d9_rgb16_source_test
    tests/model2_d3d9_rgb16_decode_test.cpp)
target_compile_features(model2_d3d9_rgb16_source_test PRIVATE cxx_std_20)
add_test(NAME model2_d3d9_verified_world_rgb16_material_source
    COMMAND model2_d3d9_rgb16_source_test)
# Same-render-thread CPU ProjectVertexToScreen -> original D3D9 UP
# candidate correlation. Never authenticates a gameplay world pass.
add_executable(model2_project_draw_join_test tests/project_draw_correlator_test.cpp)
target_compile_features(model2_project_draw_join_test PRIVATE cxx_std_20)
add_test(NAME model2_cpu_project_to_original_draw_join
    COMMAND model2_project_draw_join_test)
# Original CPU XYZ/projected-screen intrinsic projection hypothesis from
# authenticated same-thread D3D9 source triangle joins (NOT world proof).
add_executable(model2_cpu_camera_candidate_fit_test
    tests/model2_camera_projection_fit_test.cpp)
target_compile_features(model2_cpu_camera_candidate_fit_test PRIVATE cxx_std_20)
add_test(NAME model2_original_cpu_d3d9_camera_candidate_fit
    COMMAND model2_cpu_camera_candidate_fit_test)
# Preserve exact CPU projector pre-call input across nested trampoline calls.
# A returned EAX value is not a trusted pointer to ProjectVertexToScreen.
add_executable(model2_project_capture_stack_test
  tests/project_capture_stack_test.cpp)
target_compile_features(model2_project_capture_stack_test PRIVATE cxx_std_20)
add_test(NAME model2_cpu_project_original_pointer_reentrancy
  COMMAND model2_project_capture_stack_test)
# Strict source-only XYZRHW/TEX1 decode required by optional actual
# original DrawPrimitiveUP -> CPU-world -> OpenXR eye renderer route.
add_executable(model2_projected_up_world_source_test
  tests/model2_projected_up_world_source_test.cpp)
target_compile_features(model2_projected_up_world_source_test PRIVATE cxx_std_20)
add_test(NAME model2_original_projected_up_world_material_source
  COMMAND model2_projected_up_world_source_test)
# Original D3D9 Present HRESULT and frame identity guard for opt-in
# verified CPU-world renderer -> x86 IPC -> x64 OpenXR eye submission.
add_executable(model2_verified_world_present_gate_test
  tests/verified_world_present_gate_test.cpp)
target_compile_features(model2_verified_world_present_gate_test PRIVATE cxx_std_20)
add_test(NAME model2_cpu_world_original_present_commit_gate
  COMMAND model2_verified_world_present_gate_test)
# Exact D3D9 DrawPrimitive/DrawIndexedPrimitive source VB/IB index remapping.
add_executable(model2_d3d9_vb_window_test tests/model2_d3d9_vb_window_test.cpp)
target_compile_features(model2_d3d9_vb_window_test PRIVATE cxx_std_20)
target_compile_definitions(model2_d3d9_vb_window_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_real_d3d9_vb_ib_source_window COMMAND model2_d3d9_vb_window_test)
# Exact D3D9 Indexed-UP source MinVertexIndex and projected triangle order.
add_executable(model2_d3d9_indexed_up_window_test
    tests/model2_d3d9_indexed_up_window_test.cpp)
target_compile_features(model2_d3d9_indexed_up_window_test PRIVATE cxx_std_20)
target_compile_definitions(model2_d3d9_indexed_up_window_test PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_real_d3d9_indexed_up_source_window
    COMMAND model2_d3d9_indexed_up_window_test)
# Source-order exact D3D9 triangle strip/fan/list expansion for an
# authenticated CPU-world source, no emulator draw or runtime required.
add_executable(model2_d3d9_primitive_topology_test
    tests/model2_d3d9_primitive_topology_test.cpp)
target_compile_features(model2_d3d9_primitive_topology_test PRIVATE cxx_std_20)
target_compile_definitions(model2_d3d9_primitive_topology_test PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_d3d9_original_up_topology_winding
    COMMAND model2_d3d9_primitive_topology_test)
# Verify per-family first-seen caller evidence and low-overhead periodic probe.
add_executable(model2_d3d9_source_pass_context_test
    tests/model2_d3d9_source_pass_context_test.cpp)
target_compile_features(model2_d3d9_source_pass_context_test PRIVATE cxx_std_20)
add_test(NAME model2_original_d3d9_draw_source_pass_context
    COMMAND model2_d3d9_source_pass_context_test)

add_executable(model2_draw_origin_gate_test tests/draw_origin_gate_test.cpp)
target_compile_features(model2_draw_origin_gate_test PRIVATE cxx_std_20)
add_test(NAME model2_real_d3d9_caller_coverage COMMAND model2_draw_origin_gate_test)
# Preserve the game's original D3D9 frame epoch even if a secondary
# device shares the same COM vtable and invokes hook entrypoints.
add_executable(model2_primary_d3d9_owner_test
  tests/model2_primary_d3d9_owner_test.cpp)
target_compile_features(model2_primary_d3d9_owner_test PRIVATE cxx_std_20)
add_test(NAME model2_primary_d3d9_device_present_owner
  COMMAND model2_primary_d3d9_owner_test)


# Original Generic DD Model 2 emulator binding key contract (no SDL or FFB).
add_executable(model2_ffb_binding_contract_test tests/ffb_binding_contract_test.cpp)
target_compile_features(model2_ffb_binding_contract_test PRIVATE cxx_std_20)
add_test(NAME model2_real_ffb_ini_bindings COMMAND model2_ffb_binding_contract_test)

# Windows temporary-file regression: wrong GameId, only-dirty writes,
# backup, atomic swap, stale-INI conflict, and no-op preservation.
add_executable(model2vr_ffb_ini_transaction_test tests/ffb_ini_transaction_test.cpp)
target_link_libraries(model2vr_ffb_ini_transaction_test PRIVATE model2vr_ffb_ini)
target_compile_features(model2vr_ffb_ini_transaction_test PRIVATE cxx_std_20)
add_test(NAME model2_ffb_ini_transaction COMMAND model2vr_ffb_ini_transaction_test)

# Explain OpenXR-only colored eye fallback versus a genuine original
# Daytona x86 D3D9 -> GameStereo IPC -> Win64 binocular image delivery.
add_executable(model2_eye_delivery_x86_test
    tests/daytona_eye_delivery_status_test.cpp)
target_compile_features(model2_eye_delivery_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_game_eye_delivery_x86
    COMMAND model2_eye_delivery_x86_test)

# Headless host runtime health state contract, also built for x64 OpenXR host.
add_executable(model2vr_wizard_health_x86_test tests/daytona_wizard_health_test.cpp)
target_compile_features(model2vr_wizard_health_x86_test PRIVATE cxx_std_20)
add_test(NAME daytona_vr_wizard_health_x86 COMMAND model2vr_wizard_health_x86_test)

# Original-game stereo may render only against a live, coherent, genuinely
# binocular OpenXR eye-pose pair. Synthetic test, no HMD required.
add_executable(daytona_eye_pose_gate_x86_test tests/daytona_eye_pose_gate_test.cpp)
target_compile_features(daytona_eye_pose_gate_x86_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_eye_pose_gate_x86_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_live_openxr_binocular_pose_gate_x86
         COMMAND daytona_eye_pose_gate_x86_test)

# Live HMD camera-origin policy used by a separately verified world adapter.
add_executable(model2_head_recenter_x86_test tests/model2_head_recenter_test.cpp)
target_compile_features(model2_head_recenter_x86_test PRIVATE cxx_std_20)
target_compile_definitions(model2_head_recenter_x86_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_verified_camera_head_recenter_x86
         COMMAND model2_head_recenter_x86_test)

# Deterministic source OpenXR view-pose tag matching (source camera only).
add_executable(model2_pose_history_x86_test tests/stereo_pose_history_test.cpp)
target_compile_features(model2_pose_history_x86_test PRIVATE cxx_std_20)
add_test(NAME model2_exact_source_pose_history_x86
         COMMAND model2_pose_history_x86_test)

# Original native shader-UP scene candidates are gated by current D3D9
# Z, viewport, alpha/stencil/scissor and color-write state before eye shift.
add_executable(daytona_shader_scene_gate_x86_test
               tests/daytona_shader_scene_gate_test.cpp)
target_compile_features(daytona_shader_scene_gate_x86_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_shader_scene_gate_x86_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_native_shader_source_pass_gate_x86
         COMMAND daytona_shader_scene_gate_x86_test)

# Original full-size offscreen D3D9 3D source -> independent per-eye
# RT/DS -> final game backbuffer StretchRect route, one source per epoch.
add_executable(daytona_aux_surface_gate_x86_test
               tests/daytona_auxiliary_surface_gate_test.cpp)
target_compile_features(daytona_aux_surface_gate_x86_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_aux_surface_gate_x86_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_auxiliary_scene_blit_route_x86
         COMMAND daytona_aux_surface_gate_x86_test)
