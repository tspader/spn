cmake_minimum_required(VERSION 3.24)

project(spn-bootstrap C)

include(CTest)
include(ExternalProject)

if(MSVC)
  set(CMAKE_C_STANDARD 11)
  set(SPN_MSVC_COMPILE_OPTIONS
    /utf-8
  )
else()
  set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(BOOTSTRAP_DIR "${CMAKE_BINARY_DIR}")
set(BOOTSTRAP_BIN_DIR "${BOOTSTRAP_DIR}/bin")
set(BOOTSTRAP_LIB_DIR "${BOOTSTRAP_DIR}/lib")
set(BOOTSTRAP_INCLUDE_DIR "${BOOTSTRAP_DIR}/include")
set(BOOTSTRAP_EXTERNAL_DIR "${BOOTSTRAP_DIR}/external")
set(TINYCC_HEADER "${BOOTSTRAP_INCLUDE_DIR}/libtcc.h")

file(MAKE_DIRECTORY "${BOOTSTRAP_DIR}")
file(MAKE_DIRECTORY "${BOOTSTRAP_BIN_DIR}")
file(MAKE_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test")
file(MAKE_DIRECTORY "${BOOTSTRAP_LIB_DIR}")
file(MAKE_DIRECTORY "${BOOTSTRAP_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${BOOTSTRAP_EXTERNAL_DIR}")

ExternalProject_Add(argparse_external
  PREFIX "${CMAKE_BINARY_DIR}/argparse_external"
  GIT_REPOSITORY "https://github.com/tspader/argparse.git"
  GIT_TAG "master"
  GIT_PROGRESS TRUE
  GIT_SHALLOW TRUE
  GIT_SUBMODULES ""
  GIT_SUBMODULES_RECURSE FALSE
  SOURCE_DIR "${BOOTSTRAP_EXTERNAL_DIR}/argparse"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_DISCONNECTED TRUE
)

ExternalProject_Add(sp_external
  PREFIX "${CMAKE_BINARY_DIR}/sp_external"
  GIT_REPOSITORY "https://github.com/tspader/sp.git"
  GIT_TAG "main"
  GIT_PROGRESS TRUE
  GIT_SHALLOW TRUE
  GIT_SUBMODULES ""
  GIT_SUBMODULES_RECURSE FALSE
  SOURCE_DIR "${BOOTSTRAP_EXTERNAL_DIR}/sp"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_DISCONNECTED TRUE
)

ExternalProject_Add(toml_external
  PREFIX "${CMAKE_BINARY_DIR}/toml_external"
  GIT_REPOSITORY "https://github.com/tspader/toml.git"
  GIT_TAG "master"
  GIT_PROGRESS TRUE
  GIT_SHALLOW TRUE
  GIT_SUBMODULES ""
  GIT_SUBMODULES_RECURSE FALSE
  SOURCE_DIR "${BOOTSTRAP_EXTERNAL_DIR}/toml"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_DISCONNECTED TRUE
)

##############
# TINYCC     #
##############
# Detect Windows targets (including MSYS2 where cmake sees UNIX, not WIN32)
if(WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "MSYS")
  set(SPN_WIN32 TRUE)
else()
  set(SPN_WIN32 FALSE)
endif()

if(SPN_WIN32)
  # Build libtcc as a CMake static library and build the TCC runtime
  # via custom commands. Needed on Windows/MSYS (no fmemopen).
  ExternalProject_Add(tinycc_external
    PREFIX "${CMAKE_BINARY_DIR}/tinycc_external"
    GIT_REPOSITORY "https://github.com/tspader/tinycc.git"
    GIT_TAG "spn"
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
    SOURCE_DIR "${BOOTSTRAP_EXTERNAL_DIR}/tinycc"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_DISCONNECTED TRUE
  )

  set(TCC_SRC "${BOOTSTRAP_EXTERNAL_DIR}/tinycc")
  set(TCC_CONFIG_H "${TCC_SRC}/config.h")
  set(TCC_RUNTIME_DIR "${BOOTSTRAP_LIB_DIR}/tcc")

  # Create placeholder sources so CMake configure succeeds before the clone.
  # The real files arrive via ExternalProject at build time.
  if(NOT EXISTS "${TCC_SRC}/libtcc.c")
    file(MAKE_DIRECTORY "${TCC_SRC}")
    file(WRITE "${TCC_SRC}/libtcc.c" "")
    file(WRITE "${TCC_SRC}/tcc.c" "")
  endif()

  # Generate config.h for TCC after the clone.
  # Provide a fmemopen stub for MSVC (tcc_output_relocated_elf_to_mem uses it
  # but that function is not meaningful on PE targets).
  file(WRITE "${CMAKE_BINARY_DIR}/tcc_config.h.in"
"#define TCC_VERSION \"0.9.28rc\"\n\
#ifndef fmemopen\n\
#include <stdio.h>\n\
static inline FILE* fmemopen(void* buf, size_t size, const char* mode) {\n\
  (void)buf; (void)size; (void)mode;\n\
  return NULL;\n\
}\n\
#endif\n"
  )
  add_custom_command(
    OUTPUT "${TCC_CONFIG_H}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_BINARY_DIR}/tcc_config.h.in"
      "${TCC_CONFIG_H}"
    DEPENDS tinycc_external "${CMAKE_BINARY_DIR}/tcc_config.h.in"
    COMMENT "Generating TCC config.h"
  )
  add_custom_target(tcc_config DEPENDS "${TCC_CONFIG_H}")

  # Build libtcc as a static library with MSVC
  add_library(libtcc STATIC "${TCC_SRC}/libtcc.c")
  target_compile_definitions(libtcc PRIVATE
    TCC_TARGET_PE
    TCC_TARGET_X86_64
    ONE_SOURCE=1
    CONFIG_TCC_STATIC=1
  )
  target_include_directories(libtcc PRIVATE "${TCC_SRC}")
  if(MSVC)
    target_compile_options(libtcc PRIVATE
      ${SPN_MSVC_COMPILE_OPTIONS}
      /W2
      /wd4244
      /wd4267
      /wd4996
      /wd4018
      /wd4146
    )
  endif()
  add_dependencies(libtcc tinycc_external tcc_config)

  # Build tcc.exe for compiling the TCC runtime
  add_executable(tcc_bootstrap "${TCC_SRC}/tcc.c")
  target_compile_definitions(tcc_bootstrap PRIVATE
    TCC_TARGET_PE
    TCC_TARGET_X86_64
    ONE_SOURCE=0
  )
  target_include_directories(tcc_bootstrap PRIVATE "${TCC_SRC}")
  target_link_libraries(tcc_bootstrap PRIVATE libtcc)
  set_target_properties(tcc_bootstrap PROPERTIES
    OUTPUT_NAME "tcc"
    RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
  )
  if(MSVC)
    target_compile_options(tcc_bootstrap PRIVATE
      ${SPN_MSVC_COMPILE_OPTIONS}
      /W2
      /wd4244
      /wd4267
      /wd4996
      /wd4018
    )
  endif()

  set(TINYCC_LIBRARY "$<TARGET_FILE:libtcc>")

  # Copy libtcc.h to bootstrap include
  add_custom_command(
    OUTPUT "${TINYCC_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${TCC_SRC}/libtcc.h"
      "${TINYCC_HEADER}"
    DEPENDS tinycc_external
    COMMENT "Copying libtcc.h into bootstrap/include"
  )

  # Build TCC runtime and collect files for embedding
  set(TCC_RUNTIME_STAMP "${CMAKE_BINARY_DIR}/tcc_runtime.stamp")
  add_custom_command(
    OUTPUT "${TCC_RUNTIME_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TCC_RUNTIME_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TCC_RUNTIME_DIR}/include"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TCC_RUNTIME_DIR}/lib"
    # Build TCC runtime objects using tcc.exe
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/libtcc1.c" -o "${TCC_RUNTIME_DIR}/lib/libtcc1.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/crt1.c" -o "${TCC_RUNTIME_DIR}/lib/crt1.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/crt1w.c" -o "${TCC_RUNTIME_DIR}/lib/crt1w.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/wincrt1.c" -o "${TCC_RUNTIME_DIR}/lib/wincrt1.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/wincrt1w.c" -o "${TCC_RUNTIME_DIR}/lib/wincrt1w.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/dllcrt1.c" -o "${TCC_RUNTIME_DIR}/lib/dllcrt1.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/dllmain.c" -o "${TCC_RUNTIME_DIR}/lib/dllmain.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/win32/lib/chkstk.S" -o "${TCC_RUNTIME_DIR}/lib/chkstk.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/alloca.S" -o "${TCC_RUNTIME_DIR}/lib/alloca.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/alloca-bt.S" -o "${TCC_RUNTIME_DIR}/lib/alloca-bt.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/stdatomic.c" -o "${TCC_RUNTIME_DIR}/lib/stdatomic.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/atomic.S" -o "${TCC_RUNTIME_DIR}/lib/atomic.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/builtin.c" -o "${TCC_RUNTIME_DIR}/lib/builtin.o"
    # Archive into libtcc1.a
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}" -ar "${TCC_RUNTIME_DIR}/lib/libtcc1.a"
      "${TCC_RUNTIME_DIR}/lib/libtcc1.o"
      "${TCC_RUNTIME_DIR}/lib/crt1.o"
      "${TCC_RUNTIME_DIR}/lib/crt1w.o"
      "${TCC_RUNTIME_DIR}/lib/wincrt1.o"
      "${TCC_RUNTIME_DIR}/lib/wincrt1w.o"
      "${TCC_RUNTIME_DIR}/lib/dllcrt1.o"
      "${TCC_RUNTIME_DIR}/lib/dllmain.o"
      "${TCC_RUNTIME_DIR}/lib/chkstk.o"
      "${TCC_RUNTIME_DIR}/lib/alloca.o"
      "${TCC_RUNTIME_DIR}/lib/alloca-bt.o"
      "${TCC_RUNTIME_DIR}/lib/stdatomic.o"
      "${TCC_RUNTIME_DIR}/lib/atomic.o"
      "${TCC_RUNTIME_DIR}/lib/builtin.o"
    # Build extra objects
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/bcheck.c" -o "${TCC_RUNTIME_DIR}/lib/bcheck.o" -bt
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/bt-exe.c" -o "${TCC_RUNTIME_DIR}/lib/bt-exe.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/bt-log.c" -o "${TCC_RUNTIME_DIR}/lib/bt-log.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/bt-dll.c" -o "${TCC_RUNTIME_DIR}/lib/bt-dll.o"
    COMMAND "$<TARGET_FILE:tcc_bootstrap>" -B "${TCC_SRC}/win32" -I "${TCC_SRC}" -I "${TCC_SRC}/include" -c "${TCC_SRC}/lib/runmain.c" -o "${TCC_RUNTIME_DIR}/lib/runmain.o"
    # Copy .def files
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TCC_SRC}/win32/lib/kernel32.def" "${TCC_RUNTIME_DIR}/lib/kernel32.def"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TCC_SRC}/win32/lib/msvcrt.def" "${TCC_RUNTIME_DIR}/lib/msvcrt.def"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TCC_SRC}/win32/lib/user32.def" "${TCC_RUNTIME_DIR}/lib/user32.def"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TCC_SRC}/win32/lib/gdi32.def" "${TCC_RUNTIME_DIR}/lib/gdi32.def"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TCC_SRC}/win32/lib/ws2_32.def" "${TCC_RUNTIME_DIR}/lib/ws2_32.def"
    # Copy TCC headers
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${TCC_SRC}/include" "${TCC_RUNTIME_DIR}/include"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${TCC_SRC}/tcclib.h" "${TCC_RUNTIME_DIR}/include/tcclib.h"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${TCC_SRC}/win32/include" "${TCC_RUNTIME_DIR}/include"
    # Stamp file
    COMMAND "${CMAKE_COMMAND}" -E touch "${TCC_RUNTIME_STAMP}"
    DEPENDS tcc_bootstrap
    COMMENT "Building TCC runtime for embedding"
  )

  add_custom_target(tcc_runtime ALL DEPENDS "${TCC_RUNTIME_STAMP}")

else()
  # Non-MSVC (Linux, macOS, MinGW): build TCC via configure+make
  set(TINYCC_LIBRARY "${BOOTSTRAP_LIB_DIR}/libtcc.a")

  ExternalProject_Add(tinycc_external
    PREFIX "${CMAKE_BINARY_DIR}/tinycc_external"
    GIT_REPOSITORY "https://github.com/tspader/tinycc.git"
    GIT_TAG "spn"
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
    SOURCE_DIR "${BOOTSTRAP_EXTERNAL_DIR}/tinycc"
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND sh ./configure --enable-static --prefix=${BOOTSTRAP_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    UPDATE_DISCONNECTED TRUE
  )

  add_custom_command(
    OUTPUT "${TINYCC_LIBRARY}"
    COMMAND "${CMAKE_COMMAND}" -E test -f "${TINYCC_LIBRARY}"
    DEPENDS tinycc_external
    COMMENT "Verifying bootstrap libtcc.a"
  )

  add_custom_command(
    OUTPUT "${TINYCC_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E test -f "${TINYCC_HEADER}"
    DEPENDS tinycc_external
    COMMENT "Verifying bootstrap libtcc.h"
  )
endif()

####################
# BOOTSTRAP HEADERS
####################
add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/sp.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/sp/sp.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp.h"
  DEPENDS sp_external
  COMMENT "Copying sp.h into bootstrap/include"
)

add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/sp/coff.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}/sp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_SOURCE_DIR}/source/sp/coff.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/coff.h"
  COMMENT "Copying sp/coff.h into bootstrap/include/sp"
)

add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_math.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}/sp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/sp/sp/sp_math.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_math.h"
  DEPENDS sp_external
  COMMENT "Copying sp/sp_math.h into bootstrap/include/sp"
)

add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_elf.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}/sp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/sp/sp/sp_elf.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_elf.h"
  DEPENDS sp_external
  COMMENT "Copying sp/sp_elf.h into bootstrap/include/sp"
)

add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_glob.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}/sp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/sp/sp/sp_glob.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_glob.h"
  DEPENDS sp_external
  COMMENT "Copying sp/sp_glob.h into bootstrap/include/sp"
)

add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/toml.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/toml/toml.h"
    "${BOOTSTRAP_INCLUDE_DIR}/toml.h"
  DEPENDS toml_external
  COMMENT "Copying toml.h into bootstrap/include"
)

add_custom_command(
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/argparse.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/argparse/argparse.h"
    "${BOOTSTRAP_INCLUDE_DIR}/argparse.h"
  DEPENDS argparse_external
  COMMENT "Copying argparse.h into bootstrap/include"
)

add_custom_target(bootstrap_headers ALL
  DEPENDS
    "${BOOTSTRAP_INCLUDE_DIR}/sp.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_elf.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_glob.h"
    "${SP_OBJ_FORMAT_HEADER}"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/coff.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/sp_elf.h"
    "${BOOTSTRAP_INCLUDE_DIR}/toml.h"
    "${BOOTSTRAP_INCLUDE_DIR}/argparse.h"
    "${TINYCC_HEADER}"
)

###############
# EMBED TOOL  #
###############
if(MSVC)
  set(EMBED_OBJ_EXT ".obj")
else()
  set(EMBED_OBJ_EXT ".o")
endif()

add_executable(embed tools/embed.c)
set_target_properties(embed PROPERTIES
  OUTPUT_NAME "embed"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(embed PRIVATE "${BOOTSTRAP_INCLUDE_DIR}")
if(NOT MSVC)
  target_compile_options(embed PRIVATE -fno-sanitize=address)
  target_link_options(embed PRIVATE -fno-sanitize=address)
  target_link_libraries(embed PRIVATE m)
endif()
add_dependencies(embed bootstrap_headers)

add_custom_command(
  OUTPUT
    "${BOOTSTRAP_LIB_DIR}/spn.embed${EMBED_OBJ_EXT}"
    "${BOOTSTRAP_INCLUDE_DIR}/spn.embed.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_LIB_DIR}"
  COMMAND "$<TARGET_FILE:embed>"
    "${BOOTSTRAP_LIB_DIR}/tcc"
    "${BOOTSTRAP_LIB_DIR}/spn.embed${EMBED_OBJ_EXT}"
    "${BOOTSTRAP_INCLUDE_DIR}/spn.embed.h"
    "${CMAKE_SOURCE_DIR}/include/spn.h"
  DEPENDS embed "${CMAKE_SOURCE_DIR}/include/spn.h" $<$<BOOL:${SPN_WIN32}>:tcc_runtime>
  COMMENT "Generating TCC embed object and header"
)

if(NOT SPN_WIN32)
  set_property(SOURCE "${BOOTSTRAP_LIB_DIR}/spn.embed${EMBED_OBJ_EXT}" APPEND PROPERTY
    OBJECT_DEPENDS "${TINYCC_LIBRARY}"
  )
endif()

add_custom_target(spn_embed ALL
  DEPENDS
    "${BOOTSTRAP_LIB_DIR}/spn.embed${EMBED_OBJ_EXT}"
    "${BOOTSTRAP_INCLUDE_DIR}/spn.embed.h"
)

###############
# SPN SOURCES #
###############
set(SPN_SOURCES
  source/api.c
  source/api/autoconf/autoconf.c
  source/api/cmake/cmake.c
  source/api/core/core.c
  source/api/make/make.c
  source/app/app.c
  source/cli/add.c
  source/cli/build.c
  source/cli/clean.c
  source/cli/cli.c
  source/cli/copy.c
  source/cli/generate.c
  source/cli/graph.c
  source/cli/init.c
  source/cli/list.c
  source/cli/ls.c
  source/cli/manifest.c
  source/cli/publish.c
  source/cli/run.c
  source/cli/test.c
  source/cli/tool.c
  source/cli/update.c
  source/cli/which.c
  source/ctx/ctx.c
  source/enum/enum.c
  source/event/event.c
  source/event/log.c
  source/external/cc.c
  source/external/cJSON.c
  source/external/git.c
  source/external/mz.c
  source/external/obj.c
  source/external/tcc.c
  source/external/tom.c
  source/filter/filter.c
  source/gen.c
  source/git/cache.c
  source/git/key.c
  source/graph/graph.c
  source/index/cache.c
  source/index/index.c
  source/index/json.c
  source/index/publish.c
  source/intern/intern.c
  source/lock/lock.c
  source/log/log.c
  source/pkg/id.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/pkg/pkg.c
  source/profile/profile.c
  source/resolve/resolve.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/session/registry/registry.c
  source/session/session.c
  source/signal.c
  source/sp/cli.c
  source/sp/color.c
  source/sp/io.c
  source/sp/it.c
  source/sp/os.c
  source/sp/ps.c
  source/sp/str.c
  source/sp/terminal.c
  source/sp/tm.c
  source/spn.c
  source/target/mutate.c
  source/target/select.c
  source/target/target.c
  source/task/build/build.c
  source/task/build/nodes/embed.c
  source/task/build/nodes/link.c
  source/task/build/nodes/object.c
  source/task/build/nodes/package.c
  source/task/build/nodes/stamp.c
  source/task/build/nodes/user.c
  source/task/build.c
  source/task/configure.c
  source/task/generate.c
  source/task/graph.c
  source/task/resolve.c
  source/task/sync.c
  source/task/task.c
  source/task/test.c
  source/task/unit.c
  source/task/which.c
  source/toolchain/toolchain.c
  source/triple/triple.c
  source/tui/spinner.c
  source/tui/table.c
  source/tui/tui.c
  source/unit/package.c
)

add_executable(spn ${SPN_SOURCES})
set_target_properties(spn PROPERTIES
  OUTPUT_NAME "spn"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
if(MSVC)
  target_compile_options(spn PRIVATE
    ${SPN_MSVC_COMPILE_OPTIONS}
    /W3
    /wd4244
    /wd4267
    /wd4996
    /wd4018
    /wd4146
  )
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(spn PRIVATE -fno-sanitize=address -fno-sanitize=leak)
    target_link_options(spn PRIVATE -fno-sanitize=address -fno-sanitize=leak)
  endif()
endif()
target_include_directories(spn PRIVATE
  source
  source/external
  include
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(SPN_WIN32)
  target_link_libraries(spn PRIVATE "$<TARGET_FILE:libtcc>")
else()
  target_link_libraries(spn PRIVATE m "${TINYCC_LIBRARY}")
endif()
if(SPN_WIN32)
  target_link_libraries(spn PRIVATE shlwapi advapi32 shell32)
endif()
add_dependencies(spn bootstrap_headers spn_embed)
set_source_files_properties("${BOOTSTRAP_LIB_DIR}/spn.embed${EMBED_OBJ_EXT}"
  PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE
)
target_sources(spn PRIVATE "${BOOTSTRAP_LIB_DIR}/spn.embed${EMBED_OBJ_EXT}")

############
# RESOLVER #
############
add_executable(resolver
  source/ctx/ctx.c
  source/intern/intern.c
  source/enum/enum.c
  source/event/event.c
  source/external/cJSON.c
  source/external/mz.c
  source/external/git.c
  source/external/tom.c
  source/index/index.c
  source/index/json.c
  source/sp/io.c
  source/pkg/id.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/profile/profile.c
  source/session/registry/registry.c
  source/triple/triple.c
  source/resolve/resolve.c
  source/sp/str.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/target/mutate.c
  source/toolchain/toolchain.c
  test/core/resolver/main.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(resolver PROPERTIES
  OUTPUT_NAME "resolver"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(resolver PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(resolver PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(resolver PRIVATE m)
endif()
add_dependencies(resolver bootstrap_headers)

########
# LOAD #
########
add_executable(load
  source/ctx/ctx.c
  source/intern/intern.c
  source/enum/enum.c
  source/event/event.c
  source/external/tom.c
  source/pkg/id.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/target/mutate.c
  test/core/manifest.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/toml.c
)
set_target_properties(load PROPERTIES
  OUTPUT_NAME "load"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(load PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(load PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(load PRIVATE m)
endif()
add_dependencies(load bootstrap_headers)
add_test(NAME load COMMAND "$<TARGET_FILE:load>")

#########
# INDEX #
#########
add_executable(index
  source/enum/enum.c
  source/index/index.c
  source/index/json.c
  source/sp/io.c
  source/sp/str.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/external/cJSON.c
  source/external/mz.c
  source/external/git.c
  test/core/index/main.c
  test/core/index/query.c
  test/core/index/publish.c
  test/core/index/sync.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(index PROPERTIES
  OUTPUT_NAME "index"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(index PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(index PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(index PRIVATE m)
endif()
add_dependencies(index bootstrap_headers)

###########
# PUBLISH #
###########
add_executable(publish
  source/ctx/ctx.c
  source/enum/enum.c
  source/event/event.c
  source/external/cJSON.c
  source/external/git.c
  source/external/mz.c
  source/external/tom.c
  source/index/index.c
  source/index/json.c
  source/index/publish.c
  source/intern/intern.c
  source/pkg/id.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/profile/profile.c
  source/triple/triple.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/sp/io.c
  source/sp/str.c
  source/target/mutate.c
  source/toolchain/toolchain.c
  test/core/publish/main.c
  test/core/publish/publish.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(publish PROPERTIES
  OUTPUT_NAME "publish"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(publish PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(publish PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(publish PRIVATE m)
endif()
add_dependencies(publish bootstrap_headers)

########
# LOCK #
########
add_executable(lock
  source/lock/lock.c
  source/external/tom.c
  source/enum/enum.c
  source/semver/convert.c
  source/semver/parser.c
  source/sp/str.c
  test/core/lock.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(lock PROPERTIES
  OUTPUT_NAME "lock"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(lock PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(lock PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(lock PRIVATE m)
endif()
add_dependencies(lock bootstrap_headers)

#######
# GIT #
#######
add_executable(git
  source/git/cache.c
  source/git/key.c
  source/external/git.c
  test/core/git/main.c
  test/core/git/fixture.c
  test/core/git/cache.c
  test/core/git/key.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(git PROPERTIES
  OUTPUT_NAME "git"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(git PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(git PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(git PRIVATE m)
endif()
add_dependencies(git bootstrap_headers)

#########
# GRAPH #
#########
add_executable(graph
  source/graph/graph.c
  source/sp/tm.c
  test/core/graph.c
  test/tools/tmp.c
)
set_target_properties(graph PROPERTIES
  OUTPUT_NAME "graph"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(graph PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(graph PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(graph PRIVATE m)
endif()
add_dependencies(graph bootstrap_headers)

##########
# ORDERED MAP #
##########
add_executable(stable_map
  test/core/stable_map.c
)
set_target_properties(stable_map PROPERTIES
  OUTPUT_NAME "stable_map"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(stable_map PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(stable_map PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(stable_map PRIVATE m)
endif()
add_dependencies(stable_map bootstrap_headers)

##########
# ORDERED MAP #
##########
add_executable(ordered_map
  test/core/ordered_map.c
)
set_target_properties(ordered_map PROPERTIES
  OUTPUT_NAME "ordered_map"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(ordered_map PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(ordered_map PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(ordered_map PRIVATE m)
endif()
add_dependencies(ordered_map bootstrap_headers)

##########
# TRIPLE #
##########
add_executable(triple
  source/enum/enum.c
  source/triple/triple.c
  test/core/triple.c
)
set_target_properties(triple PROPERTIES
  OUTPUT_NAME "triple"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(triple PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(triple PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(triple PRIVATE m)
endif()
add_dependencies(triple bootstrap_headers)

add_executable(integration
  source/external/tom.c
  test/integration/main.c
  test/integration/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(integration PROPERTIES
  OUTPUT_NAME "integration"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}/test"
)
target_include_directories(integration PRIVATE
  source
  source/external
  include
  test/integration
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
if(MSVC)
  target_compile_options(integration PRIVATE ${SPN_MSVC_COMPILE_OPTIONS})
endif()
if(NOT MSVC)
  target_link_libraries(integration PRIVATE m)
endif()
add_dependencies(integration bootstrap_headers)

if(BUILD_TESTING)
  add_test(NAME resolver COMMAND "$<TARGET_FILE:resolver>")
  add_test(NAME index COMMAND "$<TARGET_FILE:index>")
  add_test(NAME publish COMMAND "$<TARGET_FILE:publish>")
  add_test(NAME lock COMMAND "$<TARGET_FILE:lock>")
  add_test(NAME git COMMAND "$<TARGET_FILE:git>")
  add_test(NAME graph COMMAND "$<TARGET_FILE:graph>")
  add_test(NAME triple COMMAND "$<TARGET_FILE:triple>")
  add_test(NAME integration COMMAND "$<TARGET_FILE:integration>")
endif()

add_custom_target(clone
  DEPENDS argparse_external sp_external toml_external tinycc_external
)
