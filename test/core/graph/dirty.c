#include "graph.h"

typedef struct {
  const c8* name;
  const graph_def_t* graph;
  const c8* touch [GRAPH_MAX_TOUCH];
  const c8* dirty [GRAPH_MAX_NODES];
  const c8* clean [GRAPH_MAX_NODES];
  spn_bg_err_kind_t errors [GRAPH_MAX_NODES];
} dirty_test_t;

static const dirty_test_t tests [] = {
  {
    .name = "missing_input_errors",
    .graph = &simple_linear_graph,
    .errors = { SPN_BG_ERR_MISSING_INPUT },
  },
  {
    .name = "missing_output_is_dirty",
    .graph = &simple_linear_graph,
    .touch = { "a" },
    .dirty = { "b", "compile" },
    .clean = { "a" },
  },
  {
    .name = "input_newer_than_output_is_dirty",
    .graph = &simple_linear_graph,
    .touch = { "b", "a" },
    .dirty = { "b", "compile" },
    .clean = { "a" },
  },
  {
    .name = "output_newer_than_input_is_clean",
    .graph = &simple_linear_graph,
    .touch = { "a", "b" },
    .clean = { "b", "a", "compile" },
  },
  {
    .name = "long_linear_dirty_propagates",
    .graph = &long_linear_graph,
    .touch = { "b", "c", "a" },
    .dirty = { "b", "c", "compile_b", "compile_c" },
    .clean = { "a" },
  },
  {
    .name = "long_linear_partial",
    .graph = &long_linear_graph,
    .touch = { "a", "b", "c", "b" },
    .dirty = { "c", "compile_c" },
    .clean = { "a", "b", "compile_b" },
  },
  {
    .name = "diamond_propagation",
    .graph = &diamond_graph,
    .touch = { "a" },
    .dirty = { "b", "c", "d", "compile_b", "compile_c", "join_d" },
    .clean = { "a" },
  },
  {
    .name = "fork_join_partial_missing",
    .graph = &fork_join_graph,
    .touch = { "c", "e", "d", "a" },
    .dirty = { "b", "d", "compile_b", "join_d" },
    .clean = { "a", "c", "e", "compile_e" },
  },
  {
    .name = "fork_join_partial_rhs",
    .graph = &fork_join_graph,
    .touch = { "a", "c", "e", "d", "b" },
    .dirty = { "d", "join_d" },
    .clean = { "a", "c", "e", "b", "compile_b", "compile_e" },
  },
  {
    .name = "fork_join_partial_lhs",
    .graph = &fork_join_graph,
    .touch = { "c", "a", "b", "d", "e" },
    .dirty = { "d", "join_d" },
    .clean = { "a", "c", "e", "b", "compile_b", "compile_e" },
  },
  {
    .name = "multi_output_missing_peer",
    .graph = &multi_output_graph,
    .touch = { "a", "c" },
    .dirty = { "b", "c", "split" },
    .clean = { "a" },
  },
  {
    .name = "split_join_asymmetric",
    .graph = &split_join_graph,
    .touch = { "a", "b", "c", "d", "e", "f", "a" },
    .dirty = { "f", "join_f" },
  },
  {
    .name = "asymmetric_long_fork_dirty",
    .graph = &asymmetric_fork_graph,
    .touch = { "a", "b", "c", "d", "e", "f", "g", "a" },
    .dirty = { "b", "c", "d", "e", "g", "compile_b", "compile_c", "compile_d", "compile_e", "join_g" },
    .clean = { "f" },
  },
  {
    .name = "comb_all_dirty",
    .graph = &comb_graph,
    .touch = { "a" },
    .dirty = { "b", "c", "d", "compile_b", "compile_c", "compile_d" },
    .clean = { "a" },
  },
  {
    .name = "comb_partial_dirty",
    .graph = &comb_graph,
    .touch = { "a", "b", "c", "d", "c" },
    .dirty = { "d", "compile_d" },
    .clean = { "a", "b", "c", "compile_b", "compile_c" },
  },
  {
    .name = "no_input_missing_output",
    .graph = &no_input_graph,
    .dirty = { "a", "b", "generate_a", "compile_b" },
  },
};

static sp_err_t expect_node_dirty(sp_test_t* t, built_graph_t* b, spn_bg_dirty_t* dirty, const c8* id, bool want_dirty) {
  graph_ref_t* ref = graph_ref(b, id);
  bool is_dirty =
    (ref->kind == NODE_KIND_FILE)
      ? spn_bg_is_file_dirty(dirty, ref->handle)
      : spn_bg_is_cmd_dirty(dirty, ref->handle);

  if (is_dirty != want_dirty) {
    sp_test_kv_c(t, "node", id);
    sp_test_fail(t, "expected {}, got {}",
      sp_fmt_cstr(want_dirty ? "dirty" : "clean"),
      sp_fmt_cstr(is_dirty ? "dirty" : "clean"));
    return SP_ERR;
  }

  return SP_OK;
}

sp_test_each(graph, dirty, dirty_test_t, tests, .setup = graph_setup) {
  built_graph_t b = build_graph(t, it->graph);
  apply_touches(&b, it->touch);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(b.graph);

  sp_carr_for(it->errors, i) {
    if (it->errors[i] == SPN_BG_OK) break;
    sp_expect(t, sp_da_size(dirty->errors) > i);
    if (sp_da_size(dirty->errors) > i) {
      sp_expect_eq(t, it->errors[i], dirty->errors[i].kind);
    }
  }

  sp_carr_for(it->dirty, i) {
    if (!it->dirty[i]) break;
    expect_node_dirty(t, &b, dirty, it->dirty[i], true);
  }
  sp_carr_for(it->clean, i) {
    if (!it->clean[i]) break;
    expect_node_dirty(t, &b, dirty, it->clean[i], false);
  }

  return SP_OK;
}
