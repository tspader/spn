add_executable(e2e
  ${SRC}/external/tom.c
  ${SRC}/sp/str.c
  main.c
  ${TEST_TOOLS}/harness.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(e2e)
target_include_directories(e2e PRIVATE
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(e2e PRIVATE
  SPN_TEST_ROOT="${CMAKE_SOURCE_DIR}"
  SPN_TEST_BIN="${STORE}/bin/spn"
)
target_link_libraries(e2e PRIVATE yyjson)
add_dependencies(e2e spn)
add_test(NAME e2e COMMAND "$<TARGET_FILE:e2e>")
set_tests_properties(e2e PROPERTIES LABELS e2e)
