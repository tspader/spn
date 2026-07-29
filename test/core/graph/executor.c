#include "graph.h"

typedef struct {
  const c8* name;
  const graph_def_t* graph;
  const c8* touch [GRAPH_MAX_TOUCH];
  const c8* expect [GRAPH_MAX_NODES];
} executor_test_t;

static const executor_test_t tests [] = {
  {
    .name = "simple_linear_new_input",
    .graph = &simple_linear_graph,
    .touch = { "a" },
    .expect = { "compile" },
  },
  {
    .name = "simple_linear_clean",
    .graph = &simple_linear_graph,
    .touch = { "a", "b" },
  },
  {
    .name = "long_linear_chain_propagation",
    .graph = &long_linear_graph,
    .touch = { "a" },
    .expect = { "compile_b", "compile_c" },
  },
  {
    .name = "long_linear_partial_dirty",
    .graph = &long_linear_graph,
    .touch = { "a", "b", "c", "b" },
    .expect = { "compile_c" },
  },
  {
    .name = "diamond_all_dirty",
    .graph = &diamond_graph,
    .touch = { "a" },
    .expect = { "compile_b", "compile_c", "join_d" },
  },
  {
    .name = "fork_join_partial_dirty",
    .graph = &fork_join_graph,
    .touch = { "c", "e", "d", "a" },
    .expect = { "compile_b", "join_d" },
  },
  {
    .name = "multi_output_all_dirty",
    .graph = &multi_output_graph,
    .touch = { "a" },
    .expect = { "split", "compile_d", "compile_e" },
  },
};

sp_test_each(graph, executor, executor_test_t, tests, .setup = graph_setup) {
  built_graph_t b = build_graph(t, it->graph);
  apply_touches(&b, it->touch);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(b.graph);
  spn_bg_executor_t* ex = spn_bg_executor_new(b.graph, dirty, sp_zero_s(spn_bg_executor_config_t));
  spn_bg_executor_run(ex);
  spn_bg_executor_join(ex);

  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected]);
  sp_for(i, expected) {
    sp_expect(t, sp_ht_getp(ex->completed, graph_ref(&b, it->expect[i])->handle) != SP_NULLPTR);
  }
  sp_expect_eq(t, (u32)sp_da_size(ex->ran), expected);

  spn_bg_executor_free(ex);
  return SP_OK;
}
