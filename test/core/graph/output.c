#include "graph.h"

typedef struct {
  const c8* label;
  const graph_def_t* graph;
  u32 expected_outputs;
} outputs_test_t;

void run_outputs_test(s32* utest_result, sp_test_file_manager_t* fm, outputs_test_t t) {
  built_graph_t b = build_graph(fm, t.label, t.graph);
  EXPECT_EQ((u32)sp_da_size(spn_bg_find_outputs(b.graph)), t.expected_outputs);
}

UTEST_F(graph, find_outputs_simple_linear) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_outputs_test(&ur, &ut.file_manager, (outputs_test_t) {
    .label = "find_outputs_simple_linear", .graph = &simple_linear_graph, .expected_outputs = 1,
  });
}

UTEST_F(graph, find_outputs_fork_join) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_outputs_test(&ur, &ut.file_manager, (outputs_test_t) {
    .label = "find_outputs_fork_join", .graph = &fork_join_graph, .expected_outputs = 1,
  });
}

UTEST_F(graph, find_outputs_split_join) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_outputs_test(&ur, &ut.file_manager, (outputs_test_t) {
    .label = "find_outputs_split_join", .graph = &split_join_graph, .expected_outputs = 1,
  });
}
