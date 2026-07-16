add_executable(dag
  ${SRC}/dag/dag.c
  ${SRC}/dag/lower.c
  ${SRC}/dag/wasi.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/gen/dag_output.gen.c
  ${SRC}/codegen/gen/dag_obs.gen.c
  ${SRC}/sha256/sha256.c
  ${SRC}/sp/io.c
  main.c
  action_cache.c
  digest.c
  discovery.c
  discover_exec.c
  enumeration.c
  exec.c
  file_cache.c
  glob.c
  graph.c
  key.c
  parallel.c
  run.c
  store.c
  strong_key.c
  tree.c
  wasm.c
  ${TEST_TOOLS}/tmp.c
  ${TEST_TOOLS}/sp_sim.c
  ${TEST_TOOLS}/wasm_emit.c
)
spn_test_binary(dag)
target_include_directories(dag PRIVATE ${SRC}/codegen/gen)
target_link_libraries(dag PRIVATE yyjson vmlib)
add_dependencies(dag spn_codegen_gen)
add_test(NAME dag COMMAND "$<TARGET_FILE:dag>")
add_test(NAME dag_sim COMMAND "$<TARGET_FILE:dag>")
set_tests_properties(dag_sim PROPERTIES ENVIRONMENT "SPN_TEST_SIM=1")
