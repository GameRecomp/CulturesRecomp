# SPDX-License-Identifier: MIT
add_library(rc_hle STATIC
  modules.c
  kernel32/kernel32.c
  winmm/winmm.c)
target_include_directories(rc_hle PUBLIC include)
target_link_libraries(rc_hle PUBLIC rc_runtime rc_platform PRIVATE rc_warnings)

if(RC_BUILD_TESTS)
  add_executable(test_hle tests/test_hle.c)
  target_include_directories(test_hle PRIVATE "${PROJECT_SOURCE_DIR}/runtime/tests")
  target_link_libraries(test_hle PRIVATE rc_hle rc_warnings)
  add_test(NAME hle COMMAND test_hle)
  set_tests_properties(hle PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy")
endif()
