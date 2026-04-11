add_library(Catch2_v2 INTERFACE)
target_include_directories(Catch2_v2 INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/catch2_single>
)
