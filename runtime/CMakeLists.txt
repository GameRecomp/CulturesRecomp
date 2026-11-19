# SPDX-License-Identifier: MIT
configure_file(src/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/rc_version.h" @ONLY)

add_library(rc_runtime STATIC
  src/cpu.c
  src/dispatch.c
  src/game.c
  src/hle.c
  src/log.c
  src/mem.c
  src/pe.c
  src/vfs.c
  src/x87.c)
target_include_directories(rc_runtime
  PUBLIC include
  PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(rc_runtime PRIVATE rc_warnings)
if(NOT MSVC)
  target_link_libraries(rc_runtime PUBLIC m)
endif()

if(RC_BUILD_TESTS)
  add_executable(test_runtime tests/test_runtime.c)
  target_link_libraries(test_runtime PRIVATE rc_runtime rc_warnings)
  add_test(NAME runtime COMMAND test_runtime "${CMAKE_CURRENT_BINARY_DIR}/vfs_test_root")
endif()
