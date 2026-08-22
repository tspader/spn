set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/spn.toml)

file(STRINGS ${CMAKE_SOURCE_DIR}/spn.toml SPN_VERSION_LINE REGEX "^version = ")
string(REGEX REPLACE "^version = \"([^\"]+)\"$" "\\1" SPN_VERSION "${SPN_VERSION_LINE}")
if(NOT SPN_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "failed to read version from spn.toml (matched: '${SPN_VERSION_LINE}')")
endif()

set(SPN_BUILD_CHANNEL "$ENV{SPN_BUILD_CHANNEL}")
if(SPN_BUILD_CHANNEL STREQUAL "")
  set(SPN_BUILD_CHANNEL dev)
endif()
if(NOT SPN_BUILD_CHANNEL MATCHES "^(dev|canary|stable)$")
  message(FATAL_ERROR "invalid SPN_BUILD_CHANNEL: '${SPN_BUILD_CHANNEL}'")
endif()

set(SPN_BUILD_COMMIT "$ENV{SPN_BUILD_COMMIT}")
if(SPN_BUILD_COMMIT STREQUAL "")
  execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE SPN_GIT_STATUS
    OUTPUT_VARIABLE SPN_BUILD_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(NOT SPN_GIT_STATUS EQUAL 0)
    set(SPN_BUILD_COMMIT "")
  endif()
endif()

configure_file(${CMAKE_SOURCE_DIR}/tools/cmake/build_info.h.in ${CMAKE_BINARY_DIR}/gen/build_info.gen.h @ONLY)
