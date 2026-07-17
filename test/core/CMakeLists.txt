add_executable(lock
  ${SRC}/lock/lock.c
  ${SRC}/external/tom.c
  ${SRC}/enum/enum.c
  ${SRC}/intern/intern.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/str.c
  lock.c
  ${TEST_TOOLS}/ctx.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(lock)
add_test(NAME lock COMMAND "$<TARGET_FILE:lock>")

add_executable(fs
  ${SRC}/sp/fs.c
  fs.c
)
spn_test_binary(fs)
add_test(NAME fs COMMAND "$<TARGET_FILE:fs>")

add_executable(graph
  graph/main.c
)
spn_test_binary(graph)
set_source_files_properties(graph/main.c PROPERTIES COMPILE_DEFINITIONS "SP_IMPLEMENTATION;SP_TEST_IMPLEMENTATION;SP_GRAPH_IMPLEMENTATION")
add_test(NAME graph COMMAND "$<TARGET_FILE:graph>")

add_executable(lazy
  ${SRC}/log/lazy/lazy.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  lazy.c
  ${TEST_TOOLS}/tmp.c
)
spn_test_binary(lazy)
add_test(NAME lazy COMMAND "$<TARGET_FILE:lazy>")

add_executable(transcript
  ${SRC}/event/build.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  transcript.c
)
spn_test_binary(transcript)
add_test(NAME transcript COMMAND "$<TARGET_FILE:transcript>")

add_executable(stable_map
  stable_map.c
)
spn_test_binary(stable_map)
add_test(NAME stable_map COMMAND "$<TARGET_FILE:stable_map>")

add_executable(unit
  unit.c
)
spn_test_binary(unit)
add_test(NAME unit COMMAND "$<TARGET_FILE:unit>")

add_executable(toml
  ${SRC}/toml/edit.c
  toml/main.c
  ${TEST_TOOLS}/ctx.c
  ${TEST_TOOLS}/tmp.c
)
spn_test_binary(toml)
target_compile_definitions(toml PRIVATE
  TOML_MANIFEST_DIR="test/core/toml/manifests"
  TOML_GOLDEN_DIR="test/core/toml/golden")
add_test(NAME toml COMMAND "$<TARGET_FILE:toml>")

add_executable(triple
  ${SRC}/enum/enum.c
  ${SRC}/triple/triple.c
  triple.c
)
spn_test_binary(triple)
add_test(NAME triple COMMAND "$<TARGET_FILE:triple>")

add_executable(when
  ${SRC}/enum/enum.c
  ${SRC}/when/when.c
  when.c
)
spn_test_binary(when)
add_test(NAME when COMMAND "$<TARGET_FILE:when>")

add_executable(options
  ${SRC}/enum/enum.c
  ${SRC}/event/event.c
  ${SRC}/pkg/options.c
  ${SRC}/when/when.c
  options.c
)
spn_test_binary(options)
add_test(NAME options COMMAND "$<TARGET_FILE:options>")

add_executable(pkg
  ${SRC}/ctx/ctx.c
  ${SRC}/enum/enum.c
  ${SRC}/intern/intern.c
  ${SRC}/pkg/pkg.c
  ${SRC}/pkg/mutate.c
  ${SRC}/target/mutate.c
  ${SRC}/sp/str.c
  pkg.c
)
spn_test_binary(pkg)
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
  compiler.c
  compiler/archive.c
  compiler/compile.c
  compiler/exports/render.c
  compiler/link.c
  compiler/toc.c
)
spn_test_binary(compiler_flags)
add_test(NAME compiler_flags COMMAND "$<TARGET_FILE:compiler_flags>")

add_executable(schema
  ${CMAKE_SOURCE_DIR}/tools/jtd.c
  ${SRC}/enum/enum.c
  schema.c
)
spn_test_binary(schema)
target_include_directories(schema PRIVATE ${CMAKE_SOURCE_DIR}/tools)
target_compile_definitions(schema PRIVATE
  SCHEMA_GEN_DIR="source/codegen/gen"
  SCHEMA_DIR="source/codegen/schema")
target_link_libraries(schema PRIVATE yyjson)
add_dependencies(schema spn_codegen_gen)
add_test(NAME schema COMMAND "$<TARGET_FILE:schema>")

add_executable(jtd
  ${CMAKE_SOURCE_DIR}/tools/jtd.c
  jtd/main.c
)
spn_test_binary(jtd)
target_include_directories(jtd PRIVATE ${CMAKE_SOURCE_DIR}/tools ${CMAKE_CURRENT_SOURCE_DIR}/jtd)
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
  manifest/lower/main.c
  ${SRC}/codegen/gen/config.gen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/options.c
  ${SRC}/codegen/when.c
  ${SRC}/codegen/lower.c
  ${SRC}/enum/enum.c
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
  ${CMAKE_CURRENT_SOURCE_DIR}/manifest/lower)
target_compile_definitions(manifest_lower PRIVATE
  MANIFEST_DIR="test/core/manifest/lower/manifests")
add_dependencies(manifest_lower spn_codegen_gen)
target_link_libraries(manifest_lower PRIVATE yyjson)
add_test(NAME manifest_lower COMMAND "$<TARGET_FILE:manifest_lower>")

add_executable(closure
  closure.c
  ${SRC}/target/closure.c
)
spn_test_binary(closure)
add_test(NAME closure COMMAND "$<TARGET_FILE:closure>")

add_executable(target_filter
  filter.c
  ${SRC}/filter/filter.c
)
spn_test_binary(target_filter)
add_test(NAME target_filter COMMAND "$<TARGET_FILE:target_filter>")

add_subdirectory(dag)
add_subdirectory(resolver)
add_subdirectory(index)
add_subdirectory(publish)
add_subdirectory(git)
add_subdirectory(toolchain)
