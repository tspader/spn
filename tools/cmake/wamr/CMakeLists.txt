cmake_minimum_required(VERSION 3.24)

set(WAMR_ROOT_DIR ${SOURCE_ROOT}/wamr)

if(NOT EXISTS ${WAMR_ROOT_DIR}/core/iwasm/include/wasm_export.h)
  message(FATAL_ERROR "wamr: ${WAMR_ROOT_DIR} missing — run `make fetch`")
endif()

string(REGEX REPLACE "-.*" "" WAMR_ARCH ${TRIPLE})
if(WAMR_ARCH STREQUAL "x86_64")
  set(WAMR_BUILD_TARGET "X86_64")
elseif(WAMR_ARCH STREQUAL "aarch64")
  set(WAMR_BUILD_TARGET "AARCH64")
else()
  message(FATAL_ERROR "wamr: unhandled arch ${WAMR_ARCH}")
endif()

set(CMAKE_C_EXTENSIONS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(WAMR_BUILD_PLATFORM "windows")
  set(WAMR_BUILD_FAST_JIT 0) # Unsupported on Windows
  set(WAMR_DISABLE_HW_BOUND_CHECK 1) # Only MSVC can compile SEH
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(WAMR_BUILD_PLATFORM "darwin")
  if(WAMR_ARCH STREQUAL "x86_64")
    set(WAMR_BUILD_FAST_JIT 1)
  else()
    set(WAMR_BUILD_FAST_JIT 0)
  endif()
else()
  set(WAMR_BUILD_PLATFORM "linux")
  set(WAMR_BUILD_FAST_JIT 1)
endif()
set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 0)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 0)
set(WAMR_BUILD_LIBC_WASI 1)
set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_REF_TYPES 1)
set(WAMR_BUILD_MULTI_MODULE 0)

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

# WAMR adds every -D to every source file, even their ASM trampolines which
# are treated as already assembled and not run through the preprocessor. That
# makes the defines show up as a warning, but it's just noise
foreach(src ${WAMR_RUNTIME_LIB_SOURCE})
  if(src MATCHES "\\.s$")
    set_source_files_properties(${src} PROPERTIES COMPILE_OPTIONS "-Wno-unused-command-line-argument")
  endif()
endforeach()

add_library(vmlib STATIC ${WAMR_RUNTIME_LIB_SOURCE})
target_include_directories(vmlib INTERFACE ${WAMR_ROOT_DIR}/core/iwasm/include)

if(MSVC)
  target_compile_definitions(vmlib PUBLIC WASM_RUNTIME_API_EXTERN=)
endif()
set_target_properties(vmlib PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${STORE}/lib)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_link_libraries(vmlib INTERFACE ws2_32 ntdll bcrypt)
  if(NOT MSVC)
    target_link_libraries(vmlib INTERFACE api-ms-win-core-path-l1-1-0)
  endif()
endif()
