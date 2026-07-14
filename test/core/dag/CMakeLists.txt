add_executable(dag
  ${SRC}/dag/dag.c
  ${SRC}/dag/lower.c
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
  exec.c
  file_cache.c
  key.c
  run.c
  store.c
  strong_key.c
  ${TEST_TOOLS}/tmp.c
)
spn_test_binary(dag)
target_include_directories(dag PRIVATE ${SRC}/codegen/gen)
target_link_libraries(dag PRIVATE yyjson)
add_dependencies(dag spn_codegen_gen)
add_test(NAME dag COMMAND "$<TARGET_FILE:dag>")
