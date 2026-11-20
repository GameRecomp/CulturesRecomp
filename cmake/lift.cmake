# SPDX-License-Identifier: MIT
#
# rc_add_game(<id> TITLE <title> EXES <exe>...)
#
# Creates the executable `cultures-<id>` for one game. The shell's main() is generated from
# cmake/game_main.c.in. Lifted C for the game, if present under generated/<id>/ (all variants;
# produced by `recomp lift`, never committed), is compiled in and RC_HAVE_GENERATED is defined.
# Without generated code the shell still builds and reports that the game has not been lifted.
#
# Lifting as part of the build (as in TropicoRecomp), so that building a game, from an IDE's
# debug action too, never runs code lifted by an older lifter or from older manifest hints:
#
# - the installs that are lifted are <ID>_GAME_DIRS (a list, e.g. CULTURES1_GAME_DIRS); empty
#   means orig/<id>/gog-en, else the first folder in orig/<id>/. Each lifted binary is about
#   40 MB of C, so more variants cost compile time;
# - without lifted code (no generated/<id>/set.c) they are lifted while configuring, so that
#   the sources are there to be found;
# - during the build recomp is built by cargo (nothing to do when nothing changed) and the
#   installs are lifted again when games/<id>/game.toml or recomp is newer than the last lift.
#   `recomp lift` rewrites only the files whose content changed, so only those are compiled
#   again.
#
# Off with -DRC_AUTO_LIFT=OFF; nothing happens without cargo or without the game (CI): then
# `recomp lift` is run by hand.

set(RC_GENERATED_DIR "${PROJECT_SOURCE_DIR}/generated" CACHE PATH "Where `recomp lift` writes C")
option(RC_AUTO_LIFT "Lift a game's code while configuring and again when the lifter or its manifest changed" ON)

set(RC_RECOMP_DIR "${PROJECT_SOURCE_DIR}/recomp")
set(RC_RECOMP "${RC_RECOMP_DIR}/target/release/recomp${CMAKE_EXECUTABLE_SUFFIX}")
find_program(RC_CARGO cargo)

# The installs of game `id` to lift, in `out`: <ID>_GAME_DIRS, or the default (see above).
function(rc_game_dirs id out)
  string(TOUPPER "${id}" ID)
  set(${ID}_GAME_DIRS "" CACHE STRING
      "Installs of ${id} the build lifts (list; empty: orig/${id}/gog-en, else the first folder in orig/${id})")
  set(dirs "")
  if(${ID}_GAME_DIRS)
    foreach(d IN LISTS ${ID}_GAME_DIRS)
      get_filename_component(d "${d}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
      if(IS_DIRECTORY "${d}")
        list(APPEND dirs "${d}")
      else()
        message(WARNING "${id}: ${ID}_GAME_DIRS names ${d}, which is not a folder")
      endif()
    endforeach()
  elseif(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/orig/${id}/gog-en")
    set(dirs "${PROJECT_SOURCE_DIR}/orig/${id}/gog-en")
  else()
    file(GLOB found LIST_DIRECTORIES true "${PROJECT_SOURCE_DIR}/orig/${id}/*")
    list(SORT found)
    foreach(d IN LISTS found)
      if(IS_DIRECTORY "${d}")
        set(dirs "${d}")
        break()
      endif()
    endforeach()
  endif()
  set(${out} "${dirs}" PARENT_SCOPE)
endfunction()

# recomp, built by cargo: a target every game's lift depends on.
function(rc_recomp_target)
  if(TARGET recomp_cli)
    return()
  endif()
  add_custom_target(recomp_cli
    COMMAND "${RC_CARGO}" build --release --quiet -p rc-cli --manifest-path "${RC_RECOMP_DIR}/Cargo.toml"
    BYPRODUCTS "${RC_RECOMP}"
    COMMENT "Building recomp (cargo)"
    USES_TERMINAL)
endfunction()

# Lift while configuring, when the game has no lifted code yet.
function(rc_lift_now id dirs gen_dir)
  if(EXISTS "${gen_dir}/set.c")
    return()
  endif()
  message(STATUS "cultures-${id}: lifting ${dirs} (no lifted code yet)")
  execute_process(
    COMMAND "${RC_CARGO}" build --release --quiet -p rc-cli --manifest-path "${RC_RECOMP_DIR}/Cargo.toml"
    RESULT_VARIABLE failed)
  if(NOT failed)
    execute_process(
      COMMAND "${RC_RECOMP}" lift ${dirs}
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      OUTPUT_QUIET
      RESULT_VARIABLE failed)
  endif()
  if(failed)
    message(WARNING "cultures-${id}: lifting failed; run recomp lift by hand (docs/WORKFLOWS.md)")
  endif()
endfunction()

# Lift again during the build, before `target` compiles the lifted `sources`, when the manifest
# or the lifter changed.
function(rc_lift id dirs gen_dir target sources)
  rc_recomp_target()
  set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${id}-lift.stamp")
  file(GLOB_RECURSE headers "${gen_dir}/*.h")
  add_custom_command(
    OUTPUT "${stamp}"
    BYPRODUCTS ${sources} ${headers}
    COMMAND "${RC_RECOMP}" lift ${dirs}
    COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
    DEPENDS "${PROJECT_SOURCE_DIR}/games/${id}/game.toml" "${RC_RECOMP}" recomp_cli
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Lifting ${id} again (its manifest or the lifter changed)"
    USES_TERMINAL)
  add_custom_target(${id}_lift DEPENDS "${stamp}")
  add_dependencies(${target} ${id}_lift)
endfunction()

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
  set(can_lift OFF)
  if(RC_AUTO_LIFT AND RC_CARGO AND NOT CMAKE_CROSSCOMPILING)
    rc_game_dirs(${id} game_dirs)
    if(game_dirs)
      set(can_lift ON)
      rc_lift_now(${id} "${game_dirs}" "${gen_dir}")
    endif()
  endif()

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
    if(can_lift)
      rc_lift(${id} "${game_dirs}" "${gen_dir}" ${target} "${gen_sources}")
    endif()
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
