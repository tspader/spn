add_executable(integration
  ${SRC}/external/tom.c
  ${SRC}/sp/str.c
  main.c
  harness.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(integration)
target_include_directories(integration PRIVATE
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(integration PRIVATE
  SPN_TEST_ROOT="${CMAKE_SOURCE_DIR}"
  SPN_TEST_BIN="${STORE}/bin/spn"
)
add_dependencies(integration spn)
add_test(NAME integration COMMAND "$<TARGET_FILE:integration>")

add_executable(spam
  ${SRC}/external/tom.c
  ${SRC}/sp/str.c
  spam.c
  harness.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(spam)
target_include_directories(spam PRIVATE
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(spam PRIVATE
  SPN_TEST_ROOT="${CMAKE_SOURCE_DIR}"
  SPN_TEST_BIN="${STORE}/bin/spn"
)
add_dependencies(spam spn)
add_test(NAME e2e COMMAND "$<TARGET_FILE:spam>")
set_tests_properties(e2e PROPERTIES LABELS e2e)
