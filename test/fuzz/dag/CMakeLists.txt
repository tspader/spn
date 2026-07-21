add_executable(fuzz_dag
  ${SRC}/dag/dag.c
  ${SRC}/dag/store.c
  ${SRC}/dag/run.c
  ${SRC}/dag/glob.c
  ${SRC}/dag/executor.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/gen/dag_output.gen.c
  ${SRC}/codegen/gen/dag_obs.gen.c
  ${SRC}/codegen/gen/fuzz.gen.c
  ${SRC}/external/tom.c
  ${SRC}/sha256/sha256.c
  ${SRC}/sp/fs.c
  ${SRC}/sp/io.c
  ${SRC}/intern/intern.c
  main.c
  generator.c
  journal.c
  lower.c
  model.c
  render.c
  trace.c
  ${TEST_TOOLS}/sp_sim.c
  ${TEST_TOOLS}/sp_fuzz.c
  ${TEST_TOOLS}/sp_fuzz_cli.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(fuzz_dag)
target_include_directories(fuzz_dag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SRC}/codegen/gen)
target_link_libraries(fuzz_dag PRIVATE yyjson)
add_dependencies(fuzz_dag spn_codegen_gen)
add_test(NAME fuzz_dag COMMAND "$<TARGET_FILE:fuzz_dag>")
