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
  src/probe/d3d9_probe.cpp
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
# Exact D3D9 DrawPrimitive/DrawIndexedPrimitive source VB/IB index remapping.
add_executable(model2_d3d9_vb_window_test tests/model2_d3d9_vb_window_test.cpp)
target_compile_features(model2_d3d9_vb_window_test PRIVATE cxx_std_20)
target_compile_definitions(model2_d3d9_vb_window_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_real_d3d9_vb_ib_source_window COMMAND model2_d3d9_vb_window_test)


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
