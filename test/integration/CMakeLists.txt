add_executable(integration
  ${SRC}/compiler/driver.c
  ${SRC}/compiler/gnu.c
  ${SRC}/compiler/msvc.c
  ${SRC}/enum/enum.c
  ${SRC}/error/str.c
  ${SRC}/external/tom.c
  ${SRC}/sp/str.c
  ${SRC}/triple/triple.c
  main.c
  ${TEST_TOOLS}/caps.c
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

add_executable(integration_spit
  ${SRC}/compiler/driver.c
  ${SRC}/compiler/gnu.c
  ${SRC}/compiler/msvc.c
  ${SRC}/enum/enum.c
  ${SRC}/error/str.c
  ${SRC}/external/tom.c
  ${SRC}/sp/str.c
  ${SRC}/triple/triple.c
  spit_main.c
  ${TEST_TOOLS}/caps.c
  ${TEST_TOOLS}/spit_harness.c
  ${TEST_TOOLS}/git.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(integration_spit)
target_include_directories(integration_spit PRIVATE
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(integration_spit PRIVATE
  SPN_TEST_BIN=".build/store/${TRIPLE}/bin/spn${CMAKE_EXECUTABLE_SUFFIX}"
)
target_link_libraries(integration_spit PRIVATE yyjson)
add_dependencies(integration_spit spn)
add_test(NAME integration_spit COMMAND "$<TARGET_FILE:integration_spit>")
