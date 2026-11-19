# SPDX-License-Identifier: MIT
#
# rc_add_game(<id> TITLE <title> EXES <exe>...)
#
# Creates the executable `cultures-<id>` for one game. The shell's main() is generated from
# cmake/game_main.c.in. Lifted C for the game, if present under generated/<id>/ (all variants;
# produced by `recomp lift`, never committed), is compiled in and RC_HAVE_GENERATED is defined.
# Without generated code the shell still builds and reports that the game has not been lifted.

set(RC_GENERATED_DIR "${PROJECT_SOURCE_DIR}/generated" CACHE PATH "Where `recomp lift` writes C")

function(rc_add_game id)
  cmake_parse_arguments(G "" "TITLE;GUEST_DIR" "EXES" ${ARGN})
  if(NOT G_TITLE OR NOT G_EXES)
    message(FATAL_ERROR "rc_add_game(${id}): TITLE and EXES are required")
  endif()

  set(RC_GAME_ID "${id}")
  set(RC_GAME_TITLE "${G_TITLE}")
  if(NOT G_GUEST_DIR)
    set(G_GUEST_DIR "C:\\\\Cultures")
  endif()
  set(RC_GAME_GUEST_DIR "${G_GUEST_DIR}")
  set(RC_GAME_EXES "")
  foreach(exe IN LISTS G_EXES)
    string(APPEND RC_GAME_EXES "\"${exe}\", ")
  endforeach()
  set(main_c "${CMAKE_CURRENT_BINARY_DIR}/main_${id}.c")
  configure_file("${PROJECT_SOURCE_DIR}/cmake/game_main.c.in" "${main_c}" @ONLY)

  set(target "cultures-${id}")
  add_executable(${target} "${main_c}")
  target_link_libraries(${target} PRIVATE rc_shell rc_warnings)

  set(gen_dir "${RC_GENERATED_DIR}/${id}")
  file(GLOB_RECURSE gen_sources CONFIGURE_DEPENDS "${gen_dir}/*.c")
  if(gen_sources)
    list(LENGTH gen_sources n)
    message(STATUS "${target}: ${n} generated source files")
    target_sources(${target} PRIVATE ${gen_sources})
    # Lifted code is not debugged at the C level and is slow unoptimised (as in TropicoRecomp).
    if(NOT MSVC)
      set_source_files_properties(${gen_sources} PROPERTIES COMPILE_OPTIONS "-O1")
    endif()
    target_include_directories(${target} PRIVATE "${gen_dir}")
    target_compile_definitions(${target} PRIVATE RC_HAVE_GENERATED=1)
  else()
    message(STATUS "${target}: no generated code (shell only)")
  endif()

  if(RC_BUILD_TESTS)
    add_test(NAME ${target}.version COMMAND ${target} --version)
    add_test(NAME ${target}.help COMMAND ${target} --help)
    if(NOT gen_sources)
      # Exit code 3 = "no lifted code in this build".
      add_test(NAME ${target}.not_lifted COMMAND ${target})
      set_tests_properties(${target}.not_lifted PROPERTIES
        PASS_REGULAR_EXPRESSION "no lifted code")
    endif()
  endif()
endfunction()
