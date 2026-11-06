cmake_minimum_required(VERSION 3.24)
project(Model2VROpenXRHost LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Model2VROpenXRHost requires Windows x64")
endif()
include(FetchContent)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_ALL_EXTENSIONS OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_SYSTEM_JSONCPP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(openxr
 GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
 GIT_TAG release-1.1.65
 GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(openxr)
add_executable(Model2VROpenXRHost main.cpp xr_session.cpp xr_frame.cpp
    emulator_window_capture.cpp emulator_menu_controller.cpp)
target_compile_features(Model2VROpenXRHost PRIVATE cxx_std_20)
target_compile_definitions(Model2VROpenXRHost PRIVATE
 XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D11 NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(Model2VROpenXRHost PRIVATE OpenXR::openxr_loader d3d11 dxgi user32 gdi32)
if(MSVC)
 target_compile_options(Model2VROpenXRHost PRIVATE /W4 /EHsc /permissive-)
endif()
install(TARGETS Model2VROpenXRHost RUNTIME DESTINATION .)

add_executable(model2_original_camera_lifetime_x64_test
    ../tests/model2_original_camera_lifetime_test.cpp)
target_compile_features(model2_original_camera_lifetime_x64_test PRIVATE cxx_std_20)
add_test(NAME model2_original_camera_COM_Draw_Present_lifetime_x64
         COMMAND model2_original_camera_lifetime_x64_test)

add_executable(daytona_pose_present_x64_test ../tests/daytona_source_pose_commit_gate_test.cpp)
target_compile_features(daytona_pose_present_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_pose_at_present_x64 COMMAND daytona_pose_present_x64_test)

# Same single fixed original-game cinema quad origin contract in x86/x64.
add_executable(daytona_mono_quad_anchor_x64_test
    ../tests/daytona_mono_quad_anchor_test.cpp)
target_compile_features(daytona_mono_quad_anchor_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_mono_quad_world_anchor_x64
         COMMAND daytona_mono_quad_anchor_x64_test)

# ABI contract must be identical between the 32-bit emulator producer
# and the 64-bit HMD consumer; run the same regression in both builds.
enable_testing()
add_executable(model2vr_shared_ipc_x64_test ../tests/stereo_frame_ipc_test.cpp)
target_compile_features(model2vr_shared_ipc_x64_test PRIVATE cxx_std_20)
add_test(NAME stereo_frame_transport_x64 COMMAND model2vr_shared_ipc_x64_test)
add_executable(model2vr_frame_policy_x64_test ../tests/stereo_frame_policy_test.cpp)
target_compile_features(model2vr_frame_policy_x64_test PRIVATE cxx_std_20)
add_test(NAME stereo_frame_rate_gate_x64 COMMAND model2vr_frame_policy_x64_test)


add_executable(model2_local_source_chain_x64_test
    ../tests/model2_reference_space_source_chain_test.cpp)
target_compile_features(model2_local_source_chain_x64_test PRIVATE cxx_std_20)
target_compile_definitions(model2_local_source_chain_x64_test PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_original_x64_local_recenter_to_x86_chain
    COMMAND model2_local_source_chain_x64_test)

add_executable(model2_reference_epoch_x64_test
    ../tests/model2_reference_space_epoch_test.cpp)
target_compile_features(model2_reference_epoch_x64_test PRIVATE cxx_std_20)
add_test(NAME model2_openxr_local_space_epoch_x64
    COMMAND model2_reference_epoch_x64_test)

add_executable(model2vr_pose_ipc_x64_test ../tests/stereo_pose_ipc_test.cpp)
target_compile_features(model2vr_pose_ipc_x64_test PRIVATE cxx_std_20)
add_test(NAME stereo_predicted_pose_ipc_x64 COMMAND model2vr_pose_ipc_x64_test)
add_executable(daytona_producer_health_x64_test ../tests/daytona_producer_health_test.cpp)
target_compile_features(daytona_producer_health_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_native_producer_health_ipc_x64
         COMMAND daytona_producer_health_x64_test)

add_executable(model2vr_wizard_health_x64_test ../tests/daytona_wizard_health_test.cpp)
target_compile_features(model2vr_wizard_health_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_vr_wizard_health_x64 COMMAND model2vr_wizard_health_x64_test)

# The host has a separate game-pixel readiness contract from the
# tracked-HMD/startup handshake. Validate it on Windows x64 as well.
add_executable(model2_eye_delivery_x64_test
    ../tests/daytona_eye_delivery_status_test.cpp)
target_compile_features(model2_eye_delivery_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_game_eye_delivery_x64
    COMMAND model2_eye_delivery_x64_test)

# The original-game mono fallback is intentionally distinct from
# verified GameStereo in both 32-bit and 64-bit native test builds.
add_executable(daytona_original_mono_fallback_x64_test
    ../tests/daytona_original_mono_fallback_test.cpp)
target_compile_features(daytona_original_mono_fallback_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_present_mono_fallback_x64
    COMMAND daytona_original_mono_fallback_x64_test)

add_executable(daytona_mono_content_evidence_x64_test
    ../tests/daytona_mono_content_evidence_test.cpp)
target_compile_features(daytona_mono_content_evidence_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_original_backbuffer_visibility_evidence_x64
    COMMAND daytona_mono_content_evidence_x64_test)

# Same admission contract must compile and behave identically on the
# 64-bit Quest OpenXR host and 32-bit original Daytona renderer.
add_executable(daytona_eye_pose_gate_x64_test ../tests/daytona_eye_pose_gate_test.cpp)
target_compile_features(daytona_eye_pose_gate_x64_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_eye_pose_gate_x64_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_live_openxr_binocular_pose_gate_x64
         COMMAND daytona_eye_pose_gate_x64_test)
add_executable(daytona_triangle_shift_x64_test ../tests/daytona_triangle_eye_shift_test.cpp)
target_compile_features(daytona_triangle_shift_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_verified_triangle_eye_shift_x64
         COMMAND daytona_triangle_shift_x64_test)


add_executable(daytona_surface_transfer_x64_test ../tests/daytona_surface_transfer_gate_test.cpp)
target_compile_features(daytona_surface_transfer_x64_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_surface_transfer_x64_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_surface_transfer_x64 COMMAND daytona_surface_transfer_x64_test)

add_executable(daytona_readback_scale_x64_test ../tests/daytona_readback_scale_test.cpp)
target_compile_features(daytona_readback_scale_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_native_readback_scale_x64 COMMAND daytona_readback_scale_x64_test)

# VDXR/OpenXR D3D11 returns typeless swapchain resources. Exercise the exact
# typed-RTV plus CPU RGBA UpdateSubresource path on the Windows WARP device so
# CI catches D3D11 contract regressions before an HMD run.
add_executable(daytona_d3d11_typeless_upload_x64_test
               ../tests/d3d11_openxr_typeless_upload_test.cpp)
target_compile_features(daytona_d3d11_typeless_upload_x64_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_d3d11_typeless_upload_x64_test PRIVATE
                           NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(daytona_d3d11_typeless_upload_x64_test PRIVATE d3d11)
add_test(NAME daytona_d3d11_typeless_openxr_upload_x64
         COMMAND daytona_d3d11_typeless_upload_x64_test)

# Identical verified-world recenter contract in x86 producer / x64 host.
add_executable(model2_head_recenter_x64_test ../tests/model2_head_recenter_test.cpp)
target_compile_features(model2_head_recenter_x64_test PRIVATE cxx_std_20)
target_compile_definitions(model2_head_recenter_x64_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME model2_verified_camera_head_recenter_x64
         COMMAND model2_head_recenter_x64_test)

add_executable(model2_pose_publication_x64_test ../tests/stereo_pose_publication_gate_test.cpp)
target_compile_features(model2_pose_publication_x64_test PRIVATE cxx_std_20)
target_compile_definitions(model2_pose_publication_x64_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME original_tracked_pose_transaction_x64 COMMAND model2_pose_publication_x64_test)

add_executable(model2_pose_history_x64_test ../tests/stereo_pose_history_test.cpp)
target_compile_features(model2_pose_history_x64_test PRIVATE cxx_std_20)
add_test(NAME model2_exact_source_pose_history_x64
         COMMAND model2_pose_history_x64_test)

add_executable(daytona_shader_scene_gate_x64_test
               ../tests/daytona_shader_scene_gate_test.cpp)
target_compile_features(daytona_shader_scene_gate_x64_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_shader_scene_gate_x64_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_native_shader_source_pass_gate_x64
         COMMAND daytona_shader_scene_gate_x64_test)

add_executable(daytona_aux_surface_gate_x64_test
               ../tests/daytona_auxiliary_surface_gate_test.cpp)
target_compile_features(daytona_aux_surface_gate_x64_test PRIVATE cxx_std_20)
target_compile_definitions(daytona_aux_surface_gate_x64_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME daytona_auxiliary_scene_blit_route_x64
         COMMAND daytona_aux_surface_gate_x64_test)

# Win64 OpenXR actual swapchain format preference and source-RGBA upload.
add_executable(daytona_host_pixel_format_x64_test
    ../tests/daytona_host_pixel_format_test.cpp)
target_compile_features(daytona_host_pixel_format_x64_test PRIVATE cxx_std_20)
add_test(NAME daytona_host_rgba_bgra_swapchain_x64
         COMMAND daytona_host_pixel_format_x64_test)

# Desktop emulator menu/ROM dialog video is always flat OpenXR quad,
# never a fabricated original D3D9 verified stereo source.
add_executable(model2_emulator_window_policy_x64_test
    ../tests/model2_emulator_window_policy_test.cpp)
target_compile_features(model2_emulator_window_policy_x64_test PRIVATE cxx_std_20)
add_test(NAME model2_emulator_menu_2d_pre_game_policy_x64
         COMMAND model2_emulator_window_policy_x64_test)

add_executable(model2_menu_controller_ray_x64_test
    ../tests/model2_menu_controller_ray_test.cpp)
target_compile_features(model2_menu_controller_ray_x64_test PRIVATE cxx_std_20)
add_test(NAME model2_quest_controller_menu_ray_letterbox_x64
         COMMAND model2_menu_controller_ray_x64_test)
