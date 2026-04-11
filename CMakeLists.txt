cmake_minimum_required(VERSION 3.20)

project(SimpleTimer LANGUAGES C CXX VERSION 0.9.1)

# ---------------------------------------------------------------------------------------
# Set default build to Release for single-config generators (Makefile/Ninja)
# ---------------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  if((DEFINED PROJECT_IS_TOP_LEVEL AND PROJECT_IS_TOP_LEVEL)
    OR (NOT DEFINED PROJECT_IS_TOP_LEVEL AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR))
      set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
  endif()
endif()

# simple_timer Header-only library
add_library(simple_timer INTERFACE)
add_library(simple_timer::simple_timer ALIAS simple_timer)
target_include_directories(simple_timer
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(simple_timer INTERFACE cxx_std_11)

# Compiler-specific warning/options
set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")
target_compile_options(simple_timer INTERFACE
  "$<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wall;-Wextra;-Wshadow;-Wformat=2;-Wunused>>"
  "$<${msvc_cxx}:$<BUILD_INTERFACE:/W3;/utf-8>>"
)

# ----------------------------------------------------------
# Detect master (top-level) project
# ----------------------------------------------------------
if(NOT DEFINED SIMPLE_TIMER_MASTER_PROJECT)
  if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(SIMPLE_TIMER_MASTER_PROJECT ON)
  else()
    set(SIMPLE_TIMER_MASTER_PROJECT OFF)
  endif()
endif()

if(SIMPLE_TIMER_MASTER_PROJECT)
  message(STATUS "[simple_timer] Master project: ${SIMPLE_TIMER_MASTER_PROJECT}")
  # Generate compile_commands.json for clangd/clang-tidy
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  # Enable PIC for all targets (safest for static libs) like GCC/Clang -fPIC 
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# POSIX系统下使用 pthread 库
find_package(Threads REQUIRED)
target_link_libraries(simple_timer INTERFACE Threads::Threads)


# ----------------------------------------
# Options for examples/tests/install
# ----------------------------------------
option(SIMPLE_TIMER_BUILD_EXAMPLES  "Build examples" ${SIMPLE_TIMER_MASTER_PROJECT})
option(SIMPLE_TIMER_BUILD_TESTS "Build tests" ${SIMPLE_TIMER_MASTER_PROJECT})

if(SIMPLE_TIMER_BUILD_EXAMPLES)
  message(STATUS "[simple_timer] Building examples")
  add_subdirectory(examples)
endif()
  
if(SIMPLE_TIMER_BUILD_TESTS)
  message(STATUS "[simple_timer] Building tests")
  include(CTest)
  add_subdirectory(external)
  add_subdirectory(tests)
endif()