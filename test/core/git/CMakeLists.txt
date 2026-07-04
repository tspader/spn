add_executable(git
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
spn_test_binary(git)
add_test(NAME git COMMAND "$<TARGET_FILE:git>")
