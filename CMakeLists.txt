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
