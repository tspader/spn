set(SPN_SOURCES
  core/api/api.c
  core/api/core.c
  core/core/core.c
  core/cpu/cpu.c
  core/api/node/node.c
  core/project/project.c
  core/host/host.c
  cli/commands/add.c
  cli/commands/build.c
  cli/commands/clean.c
  cli/commands/commands.c
  cli/commands/index.c
  cli/commands/init.c
  cli/commands/profile.c
  cli/commands/publish.c
  cli/commands/test.c
  cli/host/host.c
  cli/main.c
  core/codegen/codegen.c
  core/codegen/loader.c
  core/codegen/json.c
  core/codegen/lower.c
  core/codegen/options.c
  core/codegen/when.c
  core/compiler/driver.c
  core/compiler/exports.c
  core/compiler/gnu.c
  core/compiler/msvc.c
  core/compiler/toc.c
  core/ctx/ctx.c
  core/ctx/init.c
  core/dag/dag.c
  core/dag/store.c
  core/dag/run.c
  core/dag/glob.c
  core/dag/occ.c
  core/dag/stamp.c
  core/dag/wasi.c
  core/dag/wasi/canonicalize.c
  core/enum/enum.c
  core/error/error.c
  core/error/json.c
  core/event/event.c
  core/external/cc.c
  core/external/git.c
  core/external/obj.c
  core/external/tom.c
  core/external/toml.c
  core/external/wasm/abi.c
  core/external/wasm/guest.c
  core/external/wasm/wasm.c
  core/filter/filter.c
  core/git/cache.c
  core/git/key.c
  core/git/url.c
  core/git/patch.c
  core/index/cache.c
  core/index/dir.c
  core/index/release.c
  core/index/index.c
  core/index/jsonl.c
  core/index/json.c
  core/index/publish.c
  core/intern/context.c
  core/intern/intern.c
  core/lock/lock.c
  core/log/lazy/lazy.c
  core/graph/build.c
  core/graph/dag.c
  core/graph/identity.c
  core/graph/nodes/embed.c
  core/graph/nodes/link.c
  core/graph/nodes/object.c
  core/model/configure.c
  core/model/model.c
  core/model/resolve.c
  core/model/sync.c
  core/op/add.c
  core/op/build.c
  core/op/clean.c
  core/op/index.c
  core/op/op.c
  core/op/test.c
  core/op/scaffold.c
  core/op/publish.c
  core/paths/paths.c
  core/pkg/id.c
  core/pkg/mutate.c
  core/pkg/patch.c
  core/pkg/options.c
  core/pkg/pkg.c
  core/profile/profile.c
  core/reflect/json.c
  core/resolve/resolve.c
  core/semver/compare.c
  core/semver/convert.c
  core/semver/parser.c
  core/session/invocation.c
  core/session/options.c
  core/session/session.c
  core/sha256/sha256.c
  core/sp/atomic_file.c
  core/sp/coff.c
  core/sp/color.c
  core/sp/fs.c
  core/sp/io.c
  core/sp/macho.c
  core/sp/os.c
  core/sp/prompt.c
  core/sp/queue.c
  core/sp/sp.c
  core/sp/sp_cli.c
  core/sp/sp_elf.c
  core/sp/sp_glob.c
  core/sp/sp_math.c
  core/sp/sp_template.c
  core/sp/str.c
  core/target/mutate.c
  core/target/select.c
  core/target/target.c
  core/thread_pool/thread_pool.c
  core/toml/edit.c
  core/pkg/load.c
  core/toolchain/catalog.c
  core/toolchain/probe.c
  core/toolchain/provision.c
  core/toolchain/select.c
  core/toolchain/toolchain.c
  core/triple/triple.c
  cli/tui/tui.c
  core/unit/build.c
  core/unit/fingerprint.c
  core/unit/graph.c
  core/unit/link.c
  core/unit/package.c
  core/unit/paths.c
  core/unit/target.c
  core/when/when.c
)

add_executable(spn ${SPN_SOURCES}
  ${SRC}/codegen/gen/config.gen.c
  ${SRC}/codegen/gen/errors.gen.c
  ${SRC}/codegen/gen/events.gen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/gen/release.gen.c
  ${SRC}/codegen/gen/source_deps.gen.c
  ${SRC}/codegen/gen/toolchains.gen.c
  ${SRC}/codegen/gen/abi.gen.c
  )
target_include_directories(spn PRIVATE
  ${SRC}
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_SOURCE_DIR}/include
  ${SRC}/codegen/gen
  ${STORE}/include
)
add_library(spn_version STATIC core/version.c)
target_include_directories(spn_version PRIVATE ${SRC} ${STORE}/include)
add_dependencies(spn_version headers)
if(SPN_STAMP_DEFS)
  target_compile_definitions(spn_version PRIVATE ${SPN_STAMP_DEFS})
endif()

target_link_libraries(spn PRIVATE m vmlib yyjson spn_version)
# vmlib carries C++ objects (WAMR Fast JIT: asmjit + x86_64 codegen). spn's own
# sources are all C, so CMake would link with the C driver and miss the C++
# runtime; force the C++ driver (zig c++) to pull it in.
set_target_properties(spn PROPERTIES LINKER_LANGUAGE CXX)
add_dependencies(spn headers spn_embed spn_codegen_gen)

set_source_files_properties(${EMBED_OBJ} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_sources(spn PRIVATE ${EMBED_OBJ})
