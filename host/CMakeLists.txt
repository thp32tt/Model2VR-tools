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
add_executable(Model2VROpenXRHost main.cpp xr_session.cpp xr_frame.cpp)
target_compile_features(Model2VROpenXRHost PRIVATE cxx_std_20)
target_compile_definitions(Model2VROpenXRHost PRIVATE
 XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D11 NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(Model2VROpenXRHost PRIVATE OpenXR::openxr_loader d3d11 dxgi user32)
if(MSVC)
 target_compile_options(Model2VROpenXRHost PRIVATE /W4 /EHsc /permissive-)
endif()
install(TARGETS Model2VROpenXRHost RUNTIME DESTINATION .)

# ABI contract must be identical between the 32-bit emulator producer
# and the 64-bit HMD consumer; run the same regression in both builds.
enable_testing()
add_executable(model2vr_shared_ipc_x64_test ../tests/stereo_frame_ipc_test.cpp)
target_compile_features(model2vr_shared_ipc_x64_test PRIVATE cxx_std_20)
add_test(NAME stereo_frame_transport_x64 COMMAND model2vr_shared_ipc_x64_test)
add_executable(model2vr_frame_policy_x64_test ../tests/stereo_frame_policy_test.cpp)
target_compile_features(model2vr_frame_policy_x64_test PRIVATE cxx_std_20)
add_test(NAME stereo_frame_rate_gate_x64 COMMAND model2vr_frame_policy_x64_test)


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

add_executable(model2_pose_history_x64_test ../tests/stereo_pose_history_test.cpp)
target_compile_features(model2_pose_history_x64_test PRIVATE cxx_std_20)
add_test(NAME model2_exact_source_pose_history_x64
         COMMAND model2_pose_history_x64_test)
