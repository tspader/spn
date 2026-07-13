cmake_minimum_required(VERSION 3.24)

set(DEPS
  "sp|https://github.com/tspader/sp.git|9fe440334ac9ea96c924ff53efe0ce5f43e38ba7"
  "toml|https://github.com/tspader/toml.git|2e8ffdfa215cfe184fd06f8f48b47fbd04a4a678"
  "argparse|https://github.com/tspader/argparse.git|f71ed6c7b11cdbe75ffa0b42170530cc8610cbbf"
  "yyjson|https://github.com/ibireme/yyjson.git|ad58f21bee1213a8fdd614c2a11b4453815a73e9"
  "wamr|https://github.com/bytecodealliance/wasm-micro-runtime.git|e571797fbdf498d9ac4edb495205ea0f01370091"
)

if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.build/source")
endif()
file(MAKE_DIRECTORY "${SOURCE_DIR}")

set(PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}/../patches")

foreach(dep ${DEPS})
  string(REPLACE "|" ";" parts "${dep}")
  list(GET parts 0 name)
  list(GET parts 1 url)
  list(GET parts 2 commit)
  set(dest "${SOURCE_DIR}/${name}")

  set(present FALSE)
  if(EXISTS "${dest}/.git")
    execute_process(
      COMMAND git -C "${dest}" rev-parse HEAD
      OUTPUT_VARIABLE have OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(have STREQUAL commit)
      message(STATUS "fetch: ${name} already at ${commit}")
      set(present TRUE)
    endif()
  endif()

  if(NOT present)
    message(STATUS "fetch: ${name} -> ${commit}")
    file(REMOVE_RECURSE "${dest}")
    file(MAKE_DIRECTORY "${dest}")
    execute_process(COMMAND git -C "${dest}" init -q                                     RESULT_VARIABLE rc)
    if(rc EQUAL 0)
      execute_process(COMMAND git -C "${dest}" remote add origin "${url}"                RESULT_VARIABLE rc)
    endif()
    if(rc EQUAL 0)
      execute_process(COMMAND git -C "${dest}" fetch --depth 1 --quiet origin "${commit}" RESULT_VARIABLE rc)
    endif()
    if(rc EQUAL 0)
      execute_process(COMMAND git -C "${dest}" checkout -q FETCH_HEAD                    RESULT_VARIABLE rc)
    endif()
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "fetch: failed to clone ${name} (${url} @ ${commit})")
    endif()
  endif()

  file(GLOB patches "${PATCH_DIR}/${name}-*.patch")
  list(SORT patches)
  set(want "")
  foreach(patch ${patches})
    file(SHA256 "${patch}" sha)
    string(APPEND want "${sha}\n")
  endforeach()

  set(stamp "${dest}/.spn-patches")
  set(applied "")
  if(EXISTS "${stamp}")
    file(READ "${stamp}" applied)
  endif()

  if(NOT want STREQUAL applied)
    execute_process(COMMAND git -C "${dest}" checkout -q -- . RESULT_VARIABLE rc)
    foreach(patch ${patches})
      if(rc EQUAL 0)
        execute_process(COMMAND git -C "${dest}" apply "${patch}" RESULT_VARIABLE rc)
      endif()
    endforeach()
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "fetch: failed to patch ${name}")
    endif()
    file(WRITE "${stamp}" "${want}")
    if(patches)
      message(STATUS "fetch: patched ${name}")
    endif()
  endif()
endforeach()

message(STATUS "fetch: all dependencies present in ${SOURCE_DIR}")
