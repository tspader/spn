cmake_minimum_required(VERSION 3.24)

project(spn-bootstrap C)

include(CTest)
include(ExternalProject)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(BOOTSTRAP_DIR "${CMAKE_SOURCE_DIR}/bootstrap")
set(BOOTSTRAP_BIN_DIR "${BOOTSTRAP_DIR}/bin")
set(BOOTSTRAP_LIB_DIR "${BOOTSTRAP_DIR}/lib")
set(BOOTSTRAP_INCLUDE_DIR "${BOOTSTRAP_DIR}/include")
set(BOOTSTRAP_EXTERNAL_DIR "${BOOTSTRAP_DIR}/external")
set(TINYCC_LIBRARY "${BOOTSTRAP_LIB_DIR}/libtcc.a")
set(TINYCC_HEADER "${BOOTSTRAP_INCLUDE_DIR}/libtcc.h")

file(MAKE_DIRECTORY "${BOOTSTRAP_DIR}")
file(MAKE_DIRECTORY "${BOOTSTRAP_BIN_DIR}")
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
  CONFIGURE_COMMAND ./configure --enable-static --prefix=${BOOTSTRAP_DIR}
  BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
  INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
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
  OUTPUT "${BOOTSTRAP_INCLUDE_DIR}/sp/elf.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_INCLUDE_DIR}/sp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${BOOTSTRAP_EXTERNAL_DIR}/sp/sp/elf.h"
    "${BOOTSTRAP_INCLUDE_DIR}/sp/elf.h"
  DEPENDS sp_external
  COMMENT "Copying sp/elf.h into bootstrap/include/sp"
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
    "${BOOTSTRAP_INCLUDE_DIR}/sp/elf.h"
    "${BOOTSTRAP_INCLUDE_DIR}/toml.h"
    "${BOOTSTRAP_INCLUDE_DIR}/argparse.h"
    "${TINYCC_HEADER}"
)

set(SPN_SOURCES
  source/spn.c
  source/api.c
  source/app/app.c
  source/cache.c
  source/event/event.c
  source/unit/build.c
  source/unit/node.c
  source/unit/package.c
  source/signal.c
  source/graph.c
  source/tui.c
  source/ctx/ctx.c
  source/session/session.c
  source/intern.c
  source/log.c
  source/enum/enum.c
  source/index/index.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/pkg/pkg.c
  source/lock/lock.c
  source/profile.c
  source/cli.c
  source/cli/publish.c
  source/gen.c
  source/filter.c
  source/resolve/resolve.c
  source/spinner.c
  source/terminal.c
  source/external/autoconf.c
  source/external/cc.c
  source/external/cJSON.c
  source/external/cmake.c
  source/external/git.c
  source/external/make.c
  source/git/cache.c
  source/git/key.c
  source/external/mz.c
  source/external/tcc.c
  source/external/tom.c
  source/index/cache.c
  source/index/index.c
  source/index/json.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/pkg/pkg.c
  source/pkg/id.c
  source/resolve/resolve.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/sp/cli.c
  source/sp/it.c
  source/sp/tm.c
  source/sp/str.c
  source/sp/color.c
  source/sp/os.c
  source/sp/ps.c
  source/sp/io.c
  source/target/target.c
  source/target/mutate.c
  source/task/build.c
  source/task/configure.c
  source/task/graph.c
  source/task/generate.c
  source/task/resolve.c
  source/task/sync.c
  source/task/task.c
  source/task/test.c
  source/task/which.c
)

add_executable(embed tools/embed.c)
set_target_properties(embed PROPERTIES
  OUTPUT_NAME "embed"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(embed PRIVATE "${BOOTSTRAP_INCLUDE_DIR}")
target_compile_options(embed PRIVATE -fno-sanitize=address)
target_link_options(embed PRIVATE -fno-sanitize=address)
target_link_libraries(embed PRIVATE m)
add_dependencies(embed bootstrap_headers)

add_custom_command(
  OUTPUT
    "${BOOTSTRAP_LIB_DIR}/spn.embed.o"
    "${BOOTSTRAP_INCLUDE_DIR}/spn.embed.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${BOOTSTRAP_LIB_DIR}"
  COMMAND "${BOOTSTRAP_BIN_DIR}/embed"
    "${BOOTSTRAP_LIB_DIR}/tcc"
    "${BOOTSTRAP_LIB_DIR}/spn.embed.o"
    "${BOOTSTRAP_INCLUDE_DIR}/spn.embed.h"
  DEPENDS embed "${TINYCC_LIBRARY}"
  COMMENT "Generating TCC embed object and header"
)

add_custom_target(spn_embed ALL
  DEPENDS
    "${BOOTSTRAP_LIB_DIR}/spn.embed.o"
    "${BOOTSTRAP_INCLUDE_DIR}/spn.embed.h"
)

add_executable(spn ${SPN_SOURCES})
set_target_properties(spn PROPERTIES
  OUTPUT_NAME "spn"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(spn PRIVATE -fno-sanitize=address -fno-sanitize=leak)
  target_link_options(spn PRIVATE -fno-sanitize=address -fno-sanitize=leak)
endif()
target_include_directories(spn PRIVATE
  source
  source/external
  include
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(spn PRIVATE
  m
  "${TINYCC_LIBRARY}"
)
add_dependencies(spn bootstrap_headers spn_embed)
set_source_files_properties("${BOOTSTRAP_LIB_DIR}/spn.embed.o"
  PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE
)
target_sources(spn PRIVATE "${BOOTSTRAP_LIB_DIR}/spn.embed.o")

############
# RESOLVER #
############
add_executable(resolver
  source/ctx/ctx.c
  source/intern.c
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
  source/profile.c
  source/resolve/resolve.c
  source/sp/str.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/target/mutate.c
  test/core/resolver/main.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(resolver PROPERTIES
  OUTPUT_NAME "resolver"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(resolver PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(resolver PRIVATE m)
add_dependencies(resolver bootstrap_headers)

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
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(index PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(index PRIVATE m)
add_dependencies(index bootstrap_headers)

###########
# PUBLISH #
###########
add_executable(publish
  source/cli/publish.c
  source/ctx/ctx.c
  source/enum/enum.c
  source/event/event.c
  source/external/cJSON.c
  source/external/git.c
  source/external/mz.c
  source/external/tom.c
  source/index/index.c
  source/index/json.c
  source/intern.c
  source/pkg/id.c
  source/pkg/load.c
  source/pkg/mutate.c
  source/profile.c
  source/semver/compare.c
  source/semver/convert.c
  source/semver/parser.c
  source/sp/io.c
  source/sp/str.c
  source/target/mutate.c
  test/core/publish/main.c
  test/core/publish/publish.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(publish PROPERTIES
  OUTPUT_NAME "publish"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(publish PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(publish PRIVATE m)
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
  test/core/lock/main.c
  test/tools/harness.c
  test/tools/tmp.c
  test/tools/git.c
  test/tools/toml.c
)
set_target_properties(lock PROPERTIES
  OUTPUT_NAME "lock"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(lock PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(lock PRIVATE m)
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
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(git PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(git PRIVATE m)
add_dependencies(git bootstrap_headers)

#########
# GRAPH #
#########
add_executable(graph
  source/graph.c
  source/sp/tm.c
  test/core/graph.c
  test/tools/tmp.c
)
set_target_properties(graph PROPERTIES
  OUTPUT_NAME "graph"
  RUNTIME_OUTPUT_DIRECTORY "${BOOTSTRAP_BIN_DIR}"
)
target_include_directories(graph PRIVATE
  source
  include
  test/tools
  "${BOOTSTRAP_EXTERNAL_DIR}/sp/tools"
  "${BOOTSTRAP_INCLUDE_DIR}"
)
target_link_libraries(graph PRIVATE m)
add_dependencies(graph bootstrap_headers)

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
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/debug/store/bin"
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
target_link_libraries(integration PRIVATE m)
add_dependencies(integration bootstrap_headers)

if(BUILD_TESTING)
  add_test(NAME resolver COMMAND "$<TARGET_FILE:resolver>")
  add_test(NAME index COMMAND "$<TARGET_FILE:index>")
  add_test(NAME publish COMMAND "$<TARGET_FILE:publish>")
  add_test(NAME lock COMMAND "$<TARGET_FILE:lock>")
  add_test(NAME git COMMAND "$<TARGET_FILE:git>")
  add_test(NAME graph COMMAND "$<TARGET_FILE:graph>")
  add_test(NAME integration COMMAND "$<TARGET_FILE:integration>")
endif()

add_custom_target(clone
  DEPENDS argparse_external sp_external toml_external tinycc_external
)
