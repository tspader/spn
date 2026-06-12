add_executable(integration
  ${SRC}/external/tom.c
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
