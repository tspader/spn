add_executable(git_index
  ${SRC}/git/cache.c
  ${SRC}/git/key.c
  ${SRC}/git/patch.c
  ${SRC}/external/git.c
  ${SRC}/sp/fs.c
  ${SRC}/sp/str.c
  spit_main.c
  fixture.c
  cache.c
  key.c
  patch.c
  ${TEST_TOOLS}/git.c
)
spn_test_binary(git_index)
target_include_directories(git_index PRIVATE ${STORE}/include/sp)
add_test(NAME git_index COMMAND "$<TARGET_FILE:git_index>")
