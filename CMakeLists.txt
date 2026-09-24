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
