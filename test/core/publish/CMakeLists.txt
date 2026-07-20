add_executable(publish
  ${SRC}/ctx/ctx.c
  ${SRC}/enum/enum.c
  ${SRC}/event/event.c
  ${SRC}/external/cJSON.c
  ${SRC}/external/git.c
  ${SRC}/external/tom.c
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/options.c
  ${SRC}/codegen/when.c
  ${SRC}/codegen/lower.c
  ${SRC}/git/patch.c
  ${SRC}/when/when.c
  ${SRC}/toolchain/catalog.c
  ${SRC}/codegen/gen/toolchains.gen.c
  ${SRC}/codegen/gen/config.gen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/gen/release.gen.c
  ${SRC}/index/index.c
  ${SRC}/index/json.c
  ${SRC}/index/publish.c
  ${SRC}/intern/context.c
  ${SRC}/intern/intern.c
  ${SRC}/pkg/id.c
  ${SRC}/pkg/mutate.c
  ${SRC}/profile/profile.c
  ${SRC}/triple/triple.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  ${SRC}/target/mutate.c
  ${SRC}/toolchain/toolchain.c
  main.c
  publish.c
  ${TEST_TOOLS}/ctx.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(publish)
target_include_directories(publish PRIVATE ${SRC}/codegen/gen)
target_link_libraries(publish PRIVATE yyjson)
add_dependencies(publish spn_codegen_gen)
add_test(NAME publish COMMAND "$<TARGET_FILE:publish>")
