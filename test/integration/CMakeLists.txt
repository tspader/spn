add_executable(integration
  ${SRC}/external/tom.c
  ${SRC}/sp/str.c
  main.c
  ${TEST_TOOLS}/harness.c
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
  SPN_TEST_BIN=".build/store/${TRIPLE}/bin/spn${CMAKE_EXECUTABLE_SUFFIX}"
)
target_link_libraries(integration PRIVATE yyjson)
add_dependencies(integration spn)
add_test(NAME integration COMMAND "$<TARGET_FILE:integration>")
