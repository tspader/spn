cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED TRIPLE)
  message(FATAL_ERROR "TRIPLE not set — build via the top-level Makefile")
endif()
if(NOT DEFINED HOST_TRIPLE)
  set(HOST_TRIPLE ${TRIPLE})
endif()

if(NOT TRIPLE STREQUAL HOST_TRIPLE)
  string(REGEX MATCH "^[^-]+" TRIPLE_ARCH ${TRIPLE})
  if(TRIPLE MATCHES "-windows-")
    set(CMAKE_SYSTEM_NAME Windows)
  else()
    set(CMAKE_SYSTEM_NAME Linux)
  endif()
  set(CMAKE_SYSTEM_PROCESSOR ${TRIPLE_ARCH})
  set(CMAKE_C_COMPILER_TARGET ${TRIPLE})
  set(CMAKE_CXX_COMPILER_TARGET ${TRIPLE})
  set(CMAKE_ASM_COMPILER_TARGET ${TRIPLE})
endif()

if(CMAKE_HOST_WIN32 AND TRIPLE MATCHES "-windows-msvc" AND TRIPLE STREQUAL HOST_TRIPLE)
  # If we're just using cl.exe, we don't need a wrapper
else()
  set(CMAKE_C_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/zig-cc.sh)
  set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/zig-cxx.sh)
endif()
project(spn LANGUAGES C CXX)

if(MSVC)
  add_library(m INTERFACE)
  link_libraries(ws2_32)
  # Serialize PDB access; parallel cl.exe instances race on vc143.pdb otherwise
  add_compile_options(/FS)
endif()

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

# VS is a multiconfig generator, so it tries to append /$config to every
# output directory. Just pin it to the flat layout that we want.
get_property(SPN_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(SPN_MULTI_CONFIG)
  foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${STORE}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${STORE}/lib)
  endforeach()
endif()

option(SPN_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(NOT MSVC)
  if(SPN_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=undefined)
  else()
    # @spader We should nuke this ASAP
    add_compile_options(-fno-sanitize=undefined)
    add_link_options(-fno-sanitize=undefined)
  endif()
endif()

set(EMBED_OBJ ${STORE}/lib/spn.embed.o)
set(EMBED_HDR ${STORE}/include/spn.embed.h)

include(tools/cmake/libs.cmake)

add_subdirectory(tools)
add_subdirectory(source)

enable_testing()
add_subdirectory(test)

if(SPN_MULTI_CONFIG)
  # @spader More bullshit to work around multiconfig output directories; get
  # rid of all of this ASAP because it fucking sucks
  function(spn_flatten_output_dirs dir)
    get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirs})
      spn_flatten_output_dirs(${subdir})
    endforeach()
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
      get_target_property(type ${target} TYPE)
      if(type STREQUAL "INTERFACE_LIBRARY" OR type STREQUAL "UTILITY")
        continue()
      endif()
      foreach(kind RUNTIME LIBRARY ARCHIVE)
        get_target_property(out ${target} ${kind}_OUTPUT_DIRECTORY)
        if(out)
          # Only pin store paths. Tools that output into their own binary dir
          # (embed, jtd_gen) keep the /<CONFIG> subdir: it forces CMake to
          # emit `Debug\embed.exe` instead of a bare `embed.exe` in custom
          # build steps, which cmd refuses to resolve against the working
          # directory when NoDefaultCurrentDirectoryInExePath is set.
          string(FIND "${out}" "${STORE}" store_prefix)
          if(store_prefix EQUAL 0)
            foreach(config ${CMAKE_CONFIGURATION_TYPES})
              string(TOUPPER ${config} CONFIG_UPPER)
              set_target_properties(${target} PROPERTIES ${kind}_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${out})
            endforeach()
          endif()
        endif()
      endforeach()
    endforeach()
  endfunction()
  spn_flatten_output_dirs(${CMAKE_CURRENT_SOURCE_DIR})
endif()
