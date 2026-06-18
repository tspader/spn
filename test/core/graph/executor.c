#include "graph.h"

typedef struct {
  const c8* label;
  const graph_def_t* graph;
  const c8* touch[GRAPH_MAX_TOUCH];
  const c8* expected[GRAPH_MAX_NODES];
} executor_test_t;

void run_executor_test(s32* utest_result, sp_test_file_manager_t* fm, executor_test_t t) {
  built_graph_t b = build_graph(fm, t.label, t.graph);
  apply_touches(&b, t.touch);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(b.graph);
  spn_bg_executor_t* ex = spn_bg_executor_new(b.graph, dirty, sp_zero_s(spn_bg_executor_config_t));
  spn_bg_executor_run(ex);
  spn_bg_executor_join(ex);

  u32 num_expected = 0;
  for (s32 i = 0; i < GRAPH_MAX_NODES && t.expected[i]; i++) {
    num_expected++;
    EXPECT_TRUE(sp_ht_getp(ex->completed, graph_ref(&b, t.expected[i])->handle) != SP_NULLPTR);
  }
  EXPECT_EQ((u32)sp_da_size(ex->ran), num_expected);

  spn_bg_executor_free(ex);
}

UTEST_F(graph, exec_simple_linear_new_input) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_simple_linear_new_input",
    .graph = &simple_linear_graph,
    .touch = { "a" },
    .expected = { "compile" },
  });
}

UTEST_F(graph, exec_simple_linear_clean) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_simple_linear_clean",
    .graph = &simple_linear_graph,
    .touch = { "a", "b" },
  });
}

UTEST_F(graph, exec_long_linear_chain_propagation) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_long_linear_chain_propagation",
    .graph = &long_linear_graph,
    .touch = { "a" },
    .expected = { "compile_b", "compile_c" },
  });
}

UTEST_F(graph, exec_long_linear_partial_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_long_linear_partial_dirty",
    .graph = &long_linear_graph,
    .touch = { "a", "b", "c", "b" },
    .expected = { "compile_c" },
  });
}

UTEST_F(graph, exec_diamond_all_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_diamond_all_dirty",
    .graph = &diamond_graph,
    .touch = { "a" },
    .expected = { "compile_b", "compile_c", "join_d" },
  });
}

UTEST_F(graph, exec_fork_join_partial_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_fork_join_partial_dirty",
    .graph = &fork_join_graph,
    .touch = { "c", "e", "d", "a" },
    .expected = { "compile_b", "join_d" },
  });
}

UTEST_F(graph, exec_multi_output_all_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_executor_test(&ur, &ut.file_manager, (executor_test_t) {
    .label = "exec_multi_output_all_dirty",
    .graph = &multi_output_graph,
    .touch = { "a" },
    .expected = { "split", "compile_d", "compile_e" },
  });
}
