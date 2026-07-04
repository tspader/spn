cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED TRIPLE)
  message(FATAL_ERROR "TRIPLE not set — build via the top-level Makefile")
endif()

set(CMAKE_C_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/zig-cc.sh)
# CXX is only pulled in by WAMR's Fast JIT (asmjit + codegen). Pin it to zig too
# so it never falls back to system g++/libstdc++, which the zig-linked binary
# can't satisfy. Must be set before WAMR calls enable_language(CXX).
set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/zig-cxx.sh)
project(spn LANGUAGES C CXX)

# @spader Zig crashes when given --dependency-file
set(CMAKE_LINK_DEPENDS_USE_LINKER OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(SRC         ${CMAKE_SOURCE_DIR}/source)
set(SOURCE_ROOT ${CMAKE_SOURCE_DIR}/.build/source)
set(STORE       ${CMAKE_SOURCE_DIR}/.build/store/${TRIPLE})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${STORE}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${STORE}/lib)

option(SPN_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(SPN_UBSAN)
  add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=undefined)
else()
  # @spader We should nuke this ASAP
  add_compile_options(-fno-sanitize=undefined)
  add_link_options(-fno-sanitize=undefined)
endif()

set(EMBED_OBJ ${STORE}/lib/spn.embed.o)
set(EMBED_HDR ${STORE}/include/spn.embed.h)

include(tools/cmake/libs.cmake)

add_subdirectory(tools)
add_subdirectory(source)

enable_testing()
add_subdirectory(test)
