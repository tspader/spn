add_executable(toolchain_test
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/gen/toolchains.gen.c
  ${SRC}/enum/enum.c
  ${SRC}/external/tom.c
  ${SRC}/intern/intern.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  ${SRC}/toolchain/catalog.c
  ${SRC}/toolchain/provision.c
  ${SRC}/toolchain/select.c
  ${SRC}/toolchain/sha256.c
  ${SRC}/toolchain/toolchain.c
  ${SRC}/triple/triple.c
  main.c
  catalog.c
  select.c
  provision.c
  ${TEST_TOOLS}/harness.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(toolchain_test)
target_include_directories(toolchain_test PRIVATE ${SRC}/codegen/gen ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(toolchain_test PRIVATE yyjson)
add_dependencies(toolchain_test spn_codegen_gen)
add_test(NAME toolchain COMMAND "$<TARGET_FILE:toolchain_test>")
