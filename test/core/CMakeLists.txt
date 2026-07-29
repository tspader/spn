add_executable(lock
  ${SRC}/lock/lock.c
  ${SRC}/external/tom.c
  ${SRC}/enum/enum.c
  ${SRC}/intern/intern.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/str.c
  lock/spit_main.c
  lock/lock.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(lock)
target_include_directories(lock PRIVATE ${STORE}/include/sp)
add_test(NAME lock COMMAND "$<TARGET_FILE:lock>")

add_executable(fs
  ${SRC}/sp/fs.c
  fs.c
)
spn_test_binary(fs)
target_include_directories(fs PRIVATE ${STORE}/include/sp)
add_test(NAME fs COMMAND "$<TARGET_FILE:fs>")

add_executable(graph
  graph/spit_main.c
  graph/harness.c
  graph/dirty.c
  graph/executor.c
  graph/output.c
  graph/traversal.c
)
spn_test_binary(graph)
target_include_directories(graph PRIVATE ${STORE}/include/sp)
add_test(NAME graph COMMAND "$<TARGET_FILE:graph>")

add_executable(lazy
  ${SRC}/log/lazy/lazy.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  lazy.c
)
spn_test_binary(lazy)
target_include_directories(lazy PRIVATE ${STORE}/include/sp)
add_test(NAME lazy COMMAND "$<TARGET_FILE:lazy>")

add_executable(transcript
  ${SRC}/event/build.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  transcript.c
)
spn_test_binary(transcript)
target_include_directories(transcript PRIVATE ${STORE}/include/sp)
add_test(NAME transcript COMMAND "$<TARGET_FILE:transcript>")

add_executable(stable_map
  stable_map.c
)
spn_test_binary(stable_map)
target_include_directories(stable_map PRIVATE ${STORE}/include/sp)
add_test(NAME stable_map COMMAND "$<TARGET_FILE:stable_map>")

add_executable(toml
  ${SRC}/toml/edit.c
  toml/spit_main.c
  toml/harness.c
  toml/roundtrip.c
  toml/parse.c
  toml/find.c
  toml/set.c
)
spn_test_binary(toml)
target_include_directories(toml PRIVATE ${STORE}/include/sp)
add_test(NAME toml COMMAND "$<TARGET_FILE:toml>")

add_executable(triple
  ${SRC}/enum/enum.c
  ${SRC}/triple/triple.c
  triple.c
)
spn_test_binary(triple)
target_include_directories(triple PRIVATE ${STORE}/include/sp)
add_test(NAME triple COMMAND "$<TARGET_FILE:triple>")

add_executable(profile
  ${SRC}/ctx/ctx.c
  ${SRC}/enum/enum.c
  ${SRC}/intern/intern.c
  ${SRC}/profile/profile.c
  ${SRC}/triple/triple.c
  ${SRC}/sp/str.c
  profile/spit_main.c
  profile/profile.c
)
spn_test_binary(profile)
target_include_directories(profile PRIVATE ${STORE}/include/sp)
add_test(NAME profile COMMAND "$<TARGET_FILE:profile>")

add_executable(when
  ${SRC}/enum/enum.c
  ${SRC}/when/when.c
  when.c
)
spn_test_binary(when)
target_include_directories(when PRIVATE ${STORE}/include/sp)
add_test(NAME when COMMAND "$<TARGET_FILE:when>")

add_executable(options
  ${SRC}/enum/enum.c
  ${SRC}/event/event.c
  ${SRC}/intern/intern.c
  ${SRC}/pkg/options.c
  ${SRC}/session/options.c
  ${SRC}/when/when.c
  options/spit_main.c
  options/merge.c
  options/apply.c
  options/session.c
)
spn_test_binary(options)
target_include_directories(options PRIVATE ${STORE}/include/sp)
add_test(NAME options COMMAND "$<TARGET_FILE:options>")

add_executable(pkg
  ${SRC}/ctx/ctx.c
  ${SRC}/enum/enum.c
  ${SRC}/intern/intern.c
  ${SRC}/pkg/pkg.c
  ${SRC}/pkg/mutate.c
  ${SRC}/pkg/patch.c
  ${SRC}/target/mutate.c
  ${SRC}/sp/str.c
  pkg/spit_main.c
  pkg/pkg.c
)
spn_test_binary(pkg)
target_include_directories(pkg PRIVATE ${STORE}/include/sp)
add_test(NAME pkg COMMAND "$<TARGET_FILE:pkg>")

add_executable(compiler_flags
  ${SRC}/compiler/driver.c
  ${SRC}/compiler/exports.c
  ${SRC}/compiler/gnu.c
  ${SRC}/compiler/msvc.c
  ${SRC}/compiler/toc.c
  ${SRC}/enum/enum.c
  ${SRC}/sp/io.c
  ${SRC}/triple/triple.c
  compiler/spit_main.c
  compiler/harness.c
  compiler/archive.c
  compiler/compile.c
  compiler/exports/format.c
  compiler/exports/render.c
  compiler/flags.c
  compiler/link.c
  compiler/toc.c
)
spn_test_binary(compiler_flags)
target_include_directories(compiler_flags PRIVATE ${STORE}/include/sp)
add_test(NAME compiler_flags COMMAND "$<TARGET_FILE:compiler_flags>")

add_executable(schema
  ${CMAKE_SOURCE_DIR}/tools/jtd.c
  ${SRC}/enum/enum.c
  schema/spit_main.c
  schema/schema.c
)
spn_test_binary(schema)
target_include_directories(schema PRIVATE ${CMAKE_SOURCE_DIR}/tools ${STORE}/include/sp)
target_compile_definitions(schema PRIVATE
  SCHEMA_GEN_DIR="source/codegen/gen"
  SCHEMA_DIR="source/codegen/schema")
target_link_libraries(schema PRIVATE yyjson)
add_dependencies(schema spn_codegen_gen)
add_test(NAME schema COMMAND "$<TARGET_FILE:schema>")

add_executable(jtd
  ${CMAKE_SOURCE_DIR}/tools/jtd.c
  jtd/spit_main.c
  jtd/harness.c
  jtd/names.c
  jtd/type.c
  jtd/enums.c
  jtd/elements.c
  jtd/properties.c
  jtd/values.c
  jtd/discriminator.c
  jtd/ref.c
  jtd/document.c
  jtd/metadata.c
  jtd/walk.c
  jtd/conformance.c
)
spn_test_binary(jtd)
target_include_directories(jtd PRIVATE ${CMAKE_SOURCE_DIR}/tools ${CMAKE_CURRENT_SOURCE_DIR}/jtd ${STORE}/include/sp)
target_compile_definitions(jtd PRIVATE JTD_TEST_JSON_DIR="test/core/jtd/json")
target_link_libraries(jtd PRIVATE yyjson)
add_test(NAME jtd COMMAND "$<TARGET_FILE:jtd>")

add_executable(manifest_gen
  manifest/gen/main.c
  ${SRC}/codegen/gen/config.gen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/options.c
  ${SRC}/codegen/when.c
  ${SRC}/enum/enum.c
  ${SRC}/external/tom.c
  ${SRC}/intern/intern.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/str.c
  ${SRC}/sp/io.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(manifest_gen)
target_include_directories(manifest_gen PRIVATE
  ${SRC}/codegen/gen
  ${CMAKE_CURRENT_SOURCE_DIR}/manifest/gen)
target_compile_definitions(manifest_gen PRIVATE
  MANIFEST_DIR="test/core/manifest/gen/manifests"
  GOLDEN_DIR="test/core/manifest/gen/golden")
add_dependencies(manifest_gen spn_codegen_gen)
target_link_libraries(manifest_gen PRIVATE yyjson)
add_test(NAME manifest_gen COMMAND "$<TARGET_FILE:manifest_gen>")

add_executable(manifest_lower
  manifest/lower/spit_main.c
  manifest/lower/lower.c
  ${SRC}/codegen/gen/config.gen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/options.c
  ${SRC}/codegen/when.c
  ${SRC}/codegen/lower.c
  ${SRC}/enum/enum.c
  ${SRC}/git/patch.c
  ${SRC}/when/when.c
  ${SRC}/toolchain/catalog.c
  ${SRC}/codegen/gen/toolchains.gen.c
  ${SRC}/triple/triple.c
  ${SRC}/external/tom.c
  ${SRC}/intern/intern.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/str.c
  ${SRC}/sp/io.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(manifest_lower)
target_include_directories(manifest_lower PRIVATE
  ${SRC}/codegen/gen
  ${CMAKE_CURRENT_SOURCE_DIR}/manifest/lower
  ${STORE}/include/sp)
target_compile_definitions(manifest_lower PRIVATE
  MANIFEST_DIR="test/core/manifest/lower/manifests")
add_dependencies(manifest_lower spn_codegen_gen)
target_link_libraries(manifest_lower PRIVATE yyjson)
add_test(NAME manifest_lower COMMAND "$<TARGET_FILE:manifest_lower>")

add_executable(unit
  unit/spit_main.c
  unit/harness.c
  unit/walk.c
  unit/link_plan.c
  ${SRC}/unit/build.c
  ${SRC}/unit/fingerprint.c
  ${SRC}/unit/graph.c
  ${SRC}/unit/link.c
  ${SRC}/unit/paths.c
  ${SRC}/unit/target.c
  ${SRC}/session/invocation.c
  ${SRC}/session/session.c
  ${SRC}/task/build/build.c
  ${SRC}/compiler/exports.c
  ${SRC}/event/log.c
  ${SRC}/filter/filter.c
  ${SRC}/log/log.c
  ${SRC}/target/select.c
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/options.c
  ${SRC}/codegen/when.c
  ${SRC}/codegen/gen/config.gen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/gen/toolchains.gen.c
  ${SRC}/compiler/driver.c
  ${SRC}/compiler/gnu.c
  ${SRC}/compiler/msvc.c
  ${SRC}/enum/enum.c
  ${SRC}/error/str.c
  ${SRC}/external/tom.c
  ${SRC}/intern/intern.c
  ${SRC}/log/lazy/lazy.c
  ${SRC}/pkg/id.c
  ${SRC}/pkg/options.c
  ${SRC}/pkg/pkg.c
  ${SRC}/pkg/mutate.c
  ${SRC}/profile/profile.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/fs.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  ${SRC}/target/mutate.c
  ${SRC}/toolchain/catalog.c
  ${SRC}/toolchain/select.c
  ${SRC}/toolchain/toolchain.c
  ${SRC}/triple/triple.c
  ${SRC}/when/when.c
  ${SRC}/ctx/ctx.c
  ${SRC}/event/event.c
  ${TEST_TOOLS}/toml.c
  ${EMBED_OBJ}
)
set_source_files_properties(${EMBED_OBJ} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
spn_test_binary(unit)
target_include_directories(unit PRIVATE ${SRC}/codegen/gen ${SRC}/external ${SOURCE_ROOT}/wamr/core/iwasm/include ${STORE}/include/sp)
add_dependencies(unit spn_embed spn_codegen_gen)
target_link_libraries(unit PRIVATE yyjson)
add_test(NAME unit COMMAND "$<TARGET_FILE:unit>")

add_executable(closure
  closure/spit_main.c
  closure/harness.c
  closure/closure.c
  closure/dep_kind.c
  closure/get_linked_libs.c
  closure/links_code.c
  closure/runtime.c
  closure/siblings.c
  ${SRC}/unit/link.c
)
spn_test_binary(closure)
target_include_directories(closure PRIVATE ${STORE}/include/sp)
add_test(NAME closure COMMAND "$<TARGET_FILE:closure>")

add_executable(target_filter
  filter.c
  ${SRC}/filter/filter.c
)
spn_test_binary(target_filter)
target_include_directories(target_filter PRIVATE ${STORE}/include/sp)
add_test(NAME target_filter COMMAND "$<TARGET_FILE:target_filter>")

add_subdirectory(dag)
add_subdirectory(resolver)
add_subdirectory(index)
add_subdirectory(publish)
add_subdirectory(git)
add_subdirectory(toolchain)
