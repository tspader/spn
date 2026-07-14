add_executable(fuzz_dag
  ${SRC}/intern/intern.c
  main.c
  generator.c
  render.c
  ${TEST_TOOLS}/sp_sim.c
  ${TEST_TOOLS}/sp_fuzz.c
)
spn_test_binary(fuzz_dag)
target_include_directories(fuzz_dag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME fuzz_dag COMMAND "$<TARGET_FILE:fuzz_dag>")
