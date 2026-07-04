add_executable(lock
  ${SRC}/lock/lock.c
  ${SRC}/external/tom.c
  ${SRC}/enum/enum.c
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

add_executable(graph
  graph/main.c
)
spn_test_binary(graph)
target_compile_definitions(graph PRIVATE SP_IMPLEMENTATION SP_TEST_IMPLEMENTATION SP_GRAPH_IMPLEMENTATION)
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

add_executable(stable_map
  stable_map.c
)
spn_test_binary(stable_map)
add_test(NAME stable_map COMMAND "$<TARGET_FILE:stable_map>")

add_executable(triple
  ${SRC}/enum/enum.c
  ${SRC}/triple/triple.c
  triple.c
)
spn_test_binary(triple)
add_test(NAME triple COMMAND "$<TARGET_FILE:triple>")

add_executable(jtd
  ${CMAKE_SOURCE_DIR}/tools/jtd.c
  jtd/main.c
)
spn_test_binary(jtd)
target_include_directories(jtd PRIVATE ${CMAKE_SOURCE_DIR}/tools ${CMAKE_CURRENT_SOURCE_DIR}/jtd)
target_compile_definitions(jtd PRIVATE JTD_TEST_JSON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/jtd/json")
target_link_libraries(jtd PRIVATE yyjson)
add_test(NAME jtd COMMAND "$<TARGET_FILE:jtd>")

add_executable(manifest_gen
  manifest/gen/main.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/codegen.c
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
  MANIFEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/manifest/gen/manifests"
  GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/manifest/gen/golden")
add_dependencies(manifest_gen spn_codegen_gen)
add_test(NAME manifest_gen COMMAND "$<TARGET_FILE:manifest_gen>")

add_executable(manifest_lower
  manifest/lower/main.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/lower.c
  ${SRC}/enum/enum.c
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
  MANIFEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/manifest/lower/manifests")
add_dependencies(manifest_lower spn_codegen_gen)
target_link_libraries(manifest_lower PRIVATE yyjson)
add_test(NAME manifest_lower COMMAND "$<TARGET_FILE:manifest_lower>")

add_executable(closure
  closure.c
  ${SRC}/target/closure.c
)
spn_test_binary(closure)
add_test(NAME closure COMMAND "$<TARGET_FILE:closure>")

add_subdirectory(resolver)
add_subdirectory(index)
add_subdirectory(publish)
add_subdirectory(git)
add_subdirectory(toolchain)
