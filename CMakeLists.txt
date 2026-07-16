cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED TRIPLE)
  message(FATAL_ERROR "TRIPLE was not set")
endif()
if(NOT DEFINED HOST_TRIPLE)
  set(HOST_TRIPLE ${TRIPLE})
endif()

option(SPN_SANITIZE "ASan and UBSan" OFF)
if(SPN_SANITIZE AND NOT TRIPLE STREQUAL HOST_TRIPLE)
  message(FATAL_ERROR "Sanitized builds are host only (TRIPLE=${TRIPLE}, HOST_TRIPLE=${HOST_TRIPLE})")
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
elseif(SPN_SANITIZE)
  # Zig doesn't ship the sanitizer binaries, so force Clang
  find_program(SPN_SANITIZE_CC clang REQUIRED)
  find_program(SPN_SANITIZE_CXX clang++ REQUIRED)
  set(CMAKE_C_COMPILER ${SPN_SANITIZE_CC})
  set(CMAKE_CXX_COMPILER ${SPN_SANITIZE_CXX})
else()
  set(CMAKE_C_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/zig-cc.sh)
  set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/zig-cxx.sh)
endif()
project(spn LANGUAGES C CXX)

if(MSVC)
  add_library(m INTERFACE)
  link_libraries(ws2_32)
  # cl.exe needs this to serialize PDB access
  add_compile_options(/FS)
  # cl.exe defaults the source and execution charsets to the system ANSI
  # codepage; non-ASCII string literals are mangled without this
  add_compile_options(/utf-8)
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

set(SPN_FLAVOR "")
if(SPN_SANITIZE)
  set(SPN_FLAVOR "-san")
endif()

set(SRC         ${CMAKE_SOURCE_DIR}/source)
set(SOURCE_ROOT ${CMAKE_SOURCE_DIR}/.build/source)
set(STORE       ${CMAKE_SOURCE_DIR}/.build/store/${TRIPLE}${SPN_FLAVOR})

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

if(SPN_SANITIZE)
  if(MSVC)
    # MSVC has ASan but no UBSan, and /RTCx is incompatible with /fsanitize
    foreach(lang C CXX)
      string(REGEX REPLACE "/RTC[1csu]+ ?" "" CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG}")
    endforeach()
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
  else()
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
elseif(NOT MSVC)
  # Zig turns UBSan on (in trap mode) by default in debug builds; force it off
  # until the UB it catches is fixed. @spader We should nuke this ASAP
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
