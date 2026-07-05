add_executable(index
  ${SRC}/codegen/codegen.c
  ${SRC}/codegen/gen/manifest.gen.c
  ${SRC}/codegen/gen/release.gen.c
  ${SRC}/enum/enum.c
  ${SRC}/index/index.c
  ${SRC}/index/json.c
  ${SRC}/intern/intern.c
  ${SRC}/sp/io.c
  ${SRC}/sp/str.c
  ${SRC}/semver/compare.c
  ${SRC}/semver/convert.c
  ${SRC}/semver/parser.c
  ${SRC}/external/cJSON.c
  ${SRC}/external/git.c
  ${SRC}/external/tom.c
  main.c
  query.c
  publish.c
  sync.c
  ${TEST_TOOLS}/ctx.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(index)
target_include_directories(index PRIVATE ${SRC}/codegen/gen)
target_link_libraries(index PRIVATE yyjson)
add_dependencies(index spn_codegen_gen)
add_test(NAME index COMMAND "$<TARGET_FILE:index>")
