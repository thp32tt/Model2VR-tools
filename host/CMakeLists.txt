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
 GIT_TAG release-1.1.63
 GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(openxr)
add_executable(Model2VROpenXRHost main.cpp xr_session.cpp xr_frame.cpp)
target_compile_features(Model2VROpenXRHost PRIVATE cxx_std_20)
target_compile_definitions(Model2VROpenXRHost PRIVATE
 XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D11 NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(Model2VROpenXRHost PRIVATE OpenXR::openxr_loader d3d11 dxgi)
if(MSVC)
 target_compile_options(Model2VROpenXRHost PRIVATE /W4 /EHsc /permissive-)
endif()
install(TARGETS Model2VROpenXRHost RUNTIME DESTINATION .)
