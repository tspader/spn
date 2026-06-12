cmake_minimum_required(VERSION 3.24)

set(DEPS
  "sp|https://github.com/tspader/sp.git|76e98a35a6bfd954f6a41605bb559a38209268f6"
  "toml|https://github.com/tspader/toml.git|2e8ffdfa215cfe184fd06f8f48b47fbd04a4a678"
  "argparse|https://github.com/tspader/argparse.git|f71ed6c7b11cdbe75ffa0b42170530cc8610cbbf"
  "tinycc|https://github.com/tspader/tinycc.git|64fbf9ff080fac639b09b9a65c70499bcf41b581"
)

if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.build/source")
endif()
file(MAKE_DIRECTORY "${SOURCE_DIR}")

foreach(dep ${DEPS})
  string(REPLACE "|" ";" parts "${dep}")
  list(GET parts 0 name)
  list(GET parts 1 url)
  list(GET parts 2 commit)
  set(dest "${SOURCE_DIR}/${name}")

  if(EXISTS "${dest}/.git")
    execute_process(
      COMMAND git -C "${dest}" rev-parse HEAD
      OUTPUT_VARIABLE have OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(have STREQUAL commit)
      message(STATUS "fetch: ${name} already at ${commit}")
      continue()
    endif()
  endif()

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
endforeach()

message(STATUS "fetch: all dependencies present in ${SOURCE_DIR}")
