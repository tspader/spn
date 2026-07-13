add_executable(dag
  ${SRC}/dag/dag.c
  ${SRC}/sha256/sha256.c
  main.c
  ${TEST_TOOLS}/tmp.c
)
spn_test_binary(dag)
add_test(NAME dag COMMAND "$<TARGET_FILE:dag>")
