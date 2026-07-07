add_executable(git_index
  ${SRC}/git/cache.c
  ${SRC}/git/key.c
  ${SRC}/external/git.c
  ${SRC}/sp/str.c
  main.c
  fixture.c
  cache.c
  key.c
  ${TEST_TOOLS}/ctx.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(git_index)
add_test(NAME git_index COMMAND "$<TARGET_FILE:git_index>")
