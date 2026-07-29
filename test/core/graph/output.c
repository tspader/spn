#include "graph.h"

typedef struct {
  const c8* name;
  const graph_def_t* graph;
  u32 expect;
} outputs_test_t;

static const outputs_test_t tests [] = {
  { .name = "simple_linear", .graph = &simple_linear_graph, .expect = 1 },
  { .name = "fork_join",     .graph = &fork_join_graph,     .expect = 1 },
  { .name = "split_join",    .graph = &split_join_graph,    .expect = 1 },
};

sp_test_each(graph, find_outputs, outputs_test_t, tests, .setup = graph_setup) {
  built_graph_t b = graph_build(t, it->graph);
  sp_expect_eq(t, (u32)sp_da_size(spn_bg_find_outputs(b.graph)), it->expect);
  return SP_OK;
}
