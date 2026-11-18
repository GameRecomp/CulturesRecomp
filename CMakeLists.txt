# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RC_VERSION)
string(STRIP "${RC_VERSION}" RC_VERSION)
project(culturesrecomp VERSION ${RC_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RC_BUILD_TESTS "Build the unit tests" ON)
option(RC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(RC_WITH_SDL3 AUTO CACHE STRING "Use SDL3 for the platform layer (ON, OFF, AUTO)")
set_property(CACHE RC_WITH_SDL3 PROPERTY STRINGS ON OFF AUTO)

# Short git hash for logs and the window title.
set(RC_GIT_HASH "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE RC_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET RESULT_VARIABLE _git_rc)
  if(NOT _git_rc EQUAL 0)
    set(RC_GIT_HASH "unknown")
  endif()
endif()

# Warning flags shared by every target of this project.
add_library(rc_warnings INTERFACE)
if(MSVC)
  target_compile_options(rc_warnings INTERFACE /W4)
  target_compile_definitions(rc_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
  if(RC_WARNINGS_AS_ERRORS)
    target_compile_options(rc_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(rc_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes)
  if(RC_WARNINGS_AS_ERRORS)
    target_compile_options(rc_warnings INTERFACE -Werror)
  endif()
endif()

if(RC_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(runtime)
add_subdirectory(platform)
add_subdirectory(hle)

include(cmake/lift.cmake)
add_subdirectory(games/cultures1)
add_subdirectory(games/cultures2)
add_subdirectory(games/northland)
add_subdirectory(games/eighthwonder)
add_subdirectory(games/saga)
