add_executable(fuzz_dag
  ${SRC}/core/core.c
  ${SRC}/dag/dag.c
  ${SRC}/dag/store.c
  ${SRC}/dag/run.c
  ${SRC}/dag/glob.c
  ${SRC}/dag/stamp.c
  ${UTIL}/thread_pool/thread_pool.c
  ${SRC}/codegen/json.c
  ${SRC}/codegen/loader.c
  ${SRC}/codegen/gen/fuzz.gen.c
  ${SRC}/external/tom.c
  ${SRC}/hash/digest/digest.c
  ${SRC}/hash/sha256/sha256.c
  ${UTIL}/fs/fs.c
  ${UTIL}/io/io.c
  ${SRC}/intern/intern.c
  ${SRC}/paths/paths.c
  main.c
  cli.c
  generator.c
  journal.c
  lower.c
  model.c
  render.c
  trace.c
  ${TEST_TOOLS}/sim/sim.c
  ${TEST_TOOLS}/fuzz/fuzz.c
  ${TEST_TOOLS}/dag/track.c
  ${TEST_TOOLS}/toml.c
)
spn_test_binary(fuzz_dag)
target_include_directories(fuzz_dag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SRC}/codegen/gen)
target_link_libraries(fuzz_dag PRIVATE yyjson blake3)
add_dependencies(fuzz_dag spn_codegen_gen)
add_test(NAME fuzz_dag COMMAND "$<TARGET_FILE:fuzz_dag>" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
