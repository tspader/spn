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
  cli.c
  compile_commands.c
  consume.c
  corrupt.c
  cxx.c
  deps.c
  exports.c
  freshness.c
  harness_test.c
  layout.c
  log.c
  offline.c
  options.c
  patch.c
  platform.c
  profile.c
  reexport.c
  run.c
  script.c
  target.c
  units.c
  worlds.c
  tools/caps.c
  tools/harness.c
  ${TEST_TOOLS}/fixture.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(integration)
target_include_directories(integration PRIVATE
  ${SRC}/external
  ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
target_compile_definitions(integration PRIVATE
  SPN_TEST_BIN=".build/store/${TRIPLE}/bin/spn${CMAKE_EXECUTABLE_SUFFIX}"
)
target_link_libraries(integration PRIVATE yyjson)
add_dependencies(integration spn)
add_test(NAME integration COMMAND "$<TARGET_FILE:integration>")
